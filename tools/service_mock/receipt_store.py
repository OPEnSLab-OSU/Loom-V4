"""Offline model for broker receipts, project routing and database insertion timing.

SQLite represents the database in this mock. There are no network connections or secrets.
The insertion method below simulates an ACK. A live adapter needs its own real Mongo ACK path.
"""
import hashlib
import json
import sqlite3
from datetime import datetime, timezone


def utc_now():
    return datetime.now(timezone.utc).isoformat().replace('+00:00', 'Z')


class ReceiptStore:
    MAX_PAYLOAD_BYTES = 32768

    def __init__(self, filename=':memory:', projects=None, maximum_receipts=10000,
                 maximum_pending=1000, maximum_documents=10000):
        self.connection = sqlite3.connect(filename)
        self.connection.row_factory = sqlite3.Row
        self.projects = dict(projects or {'RemoteTest': 'mock://RemoteTest'})
        if min(maximum_receipts, maximum_pending, maximum_documents) < 1:
            raise ValueError('storage limits must be positive')
        self.maximum_receipts = maximum_receipts
        self.maximum_pending = maximum_pending
        self.maximum_documents = maximum_documents
        self.connection.executescript('''
            CREATE TABLE IF NOT EXISTS receipts (
                id INTEGER PRIMARY KEY, topic TEXT NOT NULL, digest TEXT NOT NULL,
                device TEXT, client_id TEXT, source_ip TEXT, project TEXT, database_name TEXT,
                packet_type TEXT, measured_utc TEXT, received_utc TEXT NOT NULL,
                inserted_utc TEXT, status TEXT NOT NULL, error TEXT, payload TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS documents (
                topic TEXT NOT NULL, digest TEXT NOT NULL, payload TEXT NOT NULL,
                inserted_utc TEXT NOT NULL, PRIMARY KEY(topic, digest));
        ''')

    def close(self):
        self.connection.close()

    def receive(self, topic, payload, client_id='', source_ip='', received_utc=None):
        """Persist receipt before attempting insertion. Duplicate arrivals remain observable."""
        if not isinstance(topic, str) or len(topic) > 208:
            raise ValueError('invalid topic')
        if not isinstance(client_id, str) or len(client_id) > 128 or not isinstance(source_ip, str) or len(source_ip) > 64:
            raise ValueError('invalid client ID or source IP')
        pending = self.connection.execute(
            "SELECT COUNT(*) FROM receipts WHERE status IN ('received','insertion_failed')").fetchone()[0]
        if pending >= self.maximum_pending:
            raise BufferError('pending receipt limit reached; apply backpressure, do not discard failed work')
        raw = payload.encode('utf-8') if isinstance(payload, str) else bytes(payload)
        if len(raw) > self.MAX_PAYLOAD_BYTES:
            raise ValueError('payload exceeds receipt limit')
        digest = hashlib.sha256(raw).hexdigest()
        text = raw.decode('utf-8', errors='replace')
        project = database = device = packet_type = measured = ''
        status, error = 'received', ''
        try:
            packet = json.loads(raw.decode('utf-8'))
            parts = topic.split('/')
            if len(parts) == 2:
                project, database, device = 'RemoteTest', *parts
            elif len(parts) == 3:
                project, database, device = parts
            else:
                raise ValueError('topic must be database/device or project/database/device')
            if any(not p or '+' in p or '#' in p for p in (project, database, device)):
                raise ValueError('invalid topic component')
            if project not in self.projects:
                raise ValueError('project has no configured database route')
            if not isinstance(packet, dict):
                raise ValueError('packet must be an object')
            identity = packet.get('id', {})
            instance = identity.get('instance')
            if not isinstance(identity.get('name'), str) or not isinstance(instance, int) or isinstance(instance, bool):
                raise ValueError('packet identity is missing or invalid')
            name = identity['name']
            if not name or len(name.encode('utf-8')) > 63 or '\0' in name or not 0 <= instance <= 2147483647:
                raise ValueError('packet identity exceeds firmware bounds')
            if identity['name'] + str(instance) != device:
                raise ValueError('topic does not match packet identity')
            kind = packet.get('type', '')
            if kind not in ('data', 'heartbeat', 'metadata'):
                raise ValueError('unsupported packet type')
            packet_type = kind
            if kind == 'heartbeat' and 'contents' in packet and packet['contents'] != []:
                raise ValueError('heartbeat contents must be an empty array')
            measured = packet.get('timestamp', {}).get('time_utc', '')
            if measured is None:
                # Firmware uses null after a checked RTC failure. Preserve the payload and
                # leave measurement time unavailable; receipt time is a separate clock.
                measured = ''
            if not isinstance(measured, str):
                measured = ''
                raise ValueError('measurement timestamp must be text')
            if measured:
                stamp = datetime.fromisoformat(measured.replace('Z', '+00:00'))
                if stamp.utcoffset() is None or stamp.utcoffset().total_seconds() != 0:
                    raise ValueError('measurement timestamp must explicitly be UTC')
        except (ValueError, TypeError, KeyError, AttributeError, UnicodeError, OverflowError, RecursionError) as exc:
            status, error = 'rejected', str(exc)
        with self.connection:
            cursor = self.connection.execute('''
                INSERT INTO receipts(topic,digest,device,client_id,source_ip,project,database_name,
                  packet_type,measured_utc,received_utc,status,error,payload)
                VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)''',
                (topic, digest, device, client_id, source_ip, project, database, packet_type,
                 measured, received_utc or utc_now(), status, error, text))
            receipt_id = cursor.lastrowid
            # Keep the configured number of arrival records; never discard uninserted work.
            self.connection.execute('''DELETE FROM receipts WHERE status IN ('inserted','duplicate','rejected')
                AND id <= (SELECT COALESCE(MAX(id),0)-? FROM receipts)''', (self.maximum_receipts,))
        return receipt_id

    def insert_mock(self, receipt_id, inserted_utc=None, fail=False):
        """Model an insertion ACK or failure without contacting MongoDB."""
        row = self.connection.execute('SELECT * FROM receipts WHERE id=?', (receipt_id,)).fetchone()
        if row is None or row['status'] == 'rejected':
            return False
        if row['status'] in ('inserted', 'duplicate'):
            return True
        with self.connection:
            if fail:
                self.connection.execute("UPDATE receipts SET status='insertion_failed', error=? WHERE id=?",
                                        ('simulated database unavailable', receipt_id))
                return False
            timestamp = inserted_utc or utc_now()
            duplicate = self.connection.execute('SELECT 1 FROM documents WHERE topic=? AND digest=?',
                                                (row['topic'], row['digest'])).fetchone()
            count = self.connection.execute('SELECT COUNT(*) FROM documents').fetchone()[0]
            if duplicate is None and count >= self.maximum_documents:
                self.connection.execute("UPDATE receipts SET status='insertion_failed',error=? WHERE id=?",
                                        ('mock document limit reached', receipt_id))
                return False
            cursor = self.connection.execute('''INSERT OR IGNORE INTO documents
                (topic,digest,payload,inserted_utc) VALUES(?,?,?,?)''',
                (row['topic'], row['digest'], row['payload'], timestamp))
            status = 'inserted' if cursor.rowcount == 1 else 'duplicate'
            self.connection.execute('UPDATE receipts SET status=?, inserted_utc=?, error=? WHERE id=?',
                                    (status, timestamp, '', receipt_id))
        return True

    def export(self):
        return [dict(row) for row in self.connection.execute('SELECT * FROM receipts ORDER BY id')]


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', help='JSONL records with topic and packet, for offline replay')
    parser.add_argument('--output', required=True, help='Receipt JSON for the local viewer')
    parser.add_argument('--project', action='append', help='Allowed project name; repeat for multiple routes')
    args = parser.parse_args()
    projects = {name: 'mock://' + name for name in args.project} if args.project else None
    store = ReceiptStore(projects=projects)
    try:
        with open(args.input, encoding='utf-8') as source:
            for line in source:
                entry = json.loads(line)
                receipt = store.receive(entry['topic'], json.dumps(entry['packet']),
                                        entry.get('client_id', ''), entry.get('source_ip', ''),
                                        entry.get('received_utc'))
                store.insert_mock(receipt, entry.get('inserted_utc'), entry.get('fail', False))
        with open(args.output, 'w', encoding='utf-8') as output:
            json.dump(store.export(), output, indent=2)
    finally:
        store.close()
