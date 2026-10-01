import json
import unittest
from receipt_store import ReceiptStore


class ReceiptTests(unittest.TestCase):
    def setUp(self):
        self.store = ReceiptStore(projects={'RemoteTest': 'mock://one', 'Vineyard': 'mock://two'})
        self.packet = {'type': 'data', 'id': {'name': 'Wisp', 'instance': 1},
                       'timestamp': {'time_utc': '2026-09-30T08:00:00Z'}, 'contents': []}

    def tearDown(self):
        self.store.close()

    def receive(self, topic='RemoteTest/Sensors/Wisp1'):
        return self.store.receive(topic, json.dumps(self.packet), client_id='feather1', source_ip='192.0.2.1')

    def test_broker_receipt_precedes_database_ack(self):
        receipt = self.receive()
        self.assertEqual(self.store.export()[0]['status'], 'received')
        self.assertFalse(self.store.insert_mock(receipt, fail=True))
        self.assertIsNone(self.store.export()[0]['inserted_utc'])
        self.assertTrue(self.store.insert_mock(receipt, '2026-09-30T08:15:00Z'))
        self.assertEqual(self.store.export()[0]['inserted_utc'], '2026-09-30T08:15:00Z')

    def test_duplicates_do_not_duplicate_insertion(self):
        first, second = self.receive(), self.receive()
        self.store.insert_mock(first)
        self.store.insert_mock(second)
        self.assertEqual([r['status'] for r in self.store.export()], ['inserted', 'duplicate'])
        self.assertEqual(self.store.connection.execute('SELECT COUNT(*) FROM documents').fetchone()[0], 1)

    def test_multiple_projects_and_legacy_topic(self):
        self.assertTrue(self.store.insert_mock(self.receive('Vineyard/Sensors/Wisp1')))
        self.assertTrue(self.store.insert_mock(self.receive('Sensors/Wisp1')))
        self.assertFalse(self.store.insert_mock(self.receive('Unknown/Sensors/Wisp1')))

    def test_heartbeat_and_metadata(self):
        for kind in ('heartbeat', 'metadata'):
            self.packet['type'] = kind
            self.assertTrue(self.store.insert_mock(self.receive()))

    def test_identity_and_timestamp_errors_are_retained(self):
        self.assertFalse(self.store.insert_mock(self.receive('RemoteTest/Sensors/Other1')))
        self.packet['timestamp']['time_utc'] = '2026-09-30T08:00:00-07:00'
        self.assertFalse(self.store.insert_mock(self.receive()))
        self.assertTrue(all(r['status'] == 'rejected' and r['error'] for r in self.store.export()))

    def test_unavailable_measurement_time_is_not_replaced_by_receipt_time(self):
        self.packet['timestamp']['time_utc'] = None
        receipt = self.receive()
        self.assertTrue(self.store.insert_mock(receipt))
        row = self.store.export()[0]
        self.assertEqual(row['measured_utc'], '')
        self.assertTrue(row['received_utc'])
        self.assertIsNone(json.loads(row['payload'])['timestamp']['time_utc'])
        self.packet.pop('timestamp')
        self.packet['type'] = 'heartbeat'
        self.assertTrue(self.store.insert_mock(self.receive()))
        self.assertEqual(self.store.export()[-1]['measured_utc'], '')

    def test_bad_json_and_size_bound(self):
        receipt = self.store.receive('Sensors/Wisp1', '{bad')
        self.assertFalse(self.store.insert_mock(receipt))
        with self.assertRaises(ValueError):
            self.store.receive('Sensors/Wisp1', b'x' * 32769)

    def test_invalid_field_types_are_logged_without_database_binding_errors(self):
        self.packet['type'] = {'invalid': 'object'}
        self.assertFalse(self.store.insert_mock(self.receive()))
        self.packet['type'] = 'data'
        self.packet['timestamp']['time_utc'] = {'invalid': 'object'}
        self.assertFalse(self.store.insert_mock(self.receive()))
        self.assertTrue(all(row['status'] == 'rejected' for row in self.store.export()))

    def test_identity_bounds_and_heartbeat_measurements_are_rejected(self):
        for name, instance in (('', 1), ('Wisp\0hidden', 1), ('a' * 64, 1),
                               ('é' * 32, 1), ('Wisp', -1), ('Wisp', 2147483648)):
            self.packet['id'] = {'name': name, 'instance': instance}
            topic = 'RemoteTest/Sensors/' + name + str(instance)
            self.assertFalse(self.store.insert_mock(self.receive(topic)))
        self.packet['id'] = {'name': 'Wisp', 'instance': 1}
        self.packet['type'] = 'heartbeat'
        self.packet['contents'] = [{'module': 'sensor', 'data': {'value': 1}}]
        self.assertFalse(self.store.insert_mock(self.receive()))

    def test_retention_preserves_failed_work(self):
        store = ReceiptStore(maximum_receipts=1)
        try:
            first = store.receive('Sensors/Wisp1', json.dumps(self.packet))
            store.insert_mock(first, fail=True)
            for _ in range(3):
                receipt = store.receive('Sensors/Wisp1', json.dumps(self.packet))
                store.insert_mock(receipt)
            self.assertEqual(store.export()[0]['id'], first)
            self.assertEqual(store.export()[0]['status'], 'insertion_failed')
        finally:
            store.close()

    def test_storage_limits_preserve_work_and_report_backpressure(self):
        store = ReceiptStore(maximum_pending=1, maximum_documents=1)
        try:
            first = store.receive('Sensors/Wisp1', json.dumps(self.packet))
            with self.assertRaises(BufferError):
                store.receive('Sensors/Wisp1', json.dumps(self.packet))
            self.assertTrue(store.insert_mock(first))
            self.packet['contents'] = [{'module': 'test', 'data': {'value': 1}}]
            second = store.receive('Sensors/Wisp1', json.dumps(self.packet))
            self.assertFalse(store.insert_mock(second))
            self.assertEqual(store.export()[-1]['status'], 'insertion_failed')
            self.assertIsNone(store.export()[-1]['inserted_utc'])
        finally:
            store.close()


if __name__ == '__main__':
    unittest.main()
