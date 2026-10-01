"""Join saved board membership, fresh discussions and reviewed local evidence without compilers."""
import argparse
import hashlib
import json
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def unique_index(rows, identifier):
    indexed = {}
    for row in rows:
        key = identifier(row)
        if key in indexed:
            raise ValueError(f'duplicate card #{key}')
        indexed[key] = row
    return indexed


def evidence_hash(relative):
    path = (ROOT / relative).resolve()
    if not path.is_relative_to(ROOT) or not path.is_file():
        raise ValueError(f'evidence must be an existing file inside Loom: {relative}')
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_audit(membership, fresh, definitions):
    board = unique_index(membership['issues'], lambda row: row['id'])
    issues = unique_index(fresh['issues'], lambda row: row['id'])
    reviewed = unique_index(definitions, lambda row: row[0])
    if set(board) != set(issues) or set(board) != set(reviewed):
        raise ValueError('membership, fresh discussions and reviewed definitions must cover the same cards')
    if membership['project_url'] != fresh['project_url']:
        raise ValueError('board URLs do not match')
    rows = []
    for number, saved in board.items():
        discussion = issues[number]
        issue, comments = discussion['issue'], discussion['comments']
        definition = reviewed[number]
        if issue['issue_number'] != number or issue['url'] != saved['url']:
            raise ValueError(f'issue identity mismatch for #{number}')
        if len(comments) != issue['comments']:
            raise ValueError(f'comment count changed or capture incomplete for #{number}; refresh it')
        _, disposition, paths, addressed, remaining = definition
        if not paths or not addressed or not remaining:
            raise ValueError(f'#{number} needs evidence, scope and remaining-work explanation')
        rows.append({'id': number, 'title': issue['title'], 'url': issue['url'],
                     'saved_board_column': saved['board_status'], 'github_state': issue['state'],
                     'issue_updated_utc': issue['updated_at'], 'comment_count': len(comments),
                     'comment_urls': [comment['url'] for comment in comments],
                     'issue_body_sha256': hashlib.sha256((issue['body'] or '').encode()).hexdigest(),
                     'disposition': disposition, 'addressed': addressed, 'remaining': remaining,
                     'evidence_sha256': {path: evidence_hash(path) for path in paths}})
    return {'saved_utc': datetime.now(timezone.utc).isoformat(),
            'project_url': membership['project_url'], 'branch': membership['branch'],
            'membership_captured_utc': membership['captured_at_utc'],
            'discussions_captured_utc': fresh['captured_at_utc'],
            'membership_refreshed': False, 'compiler_run': False, 'remote_status_changed': False,
            'scope': 'Every card in the saved 55-card board membership; source evidence is not acceptance.',
            'counts': dict(sorted(Counter(row['disposition'] for row in rows).items())), 'cards': rows}


def markdown(audit):
    lines = ['# Addressable-card audit', '',
             f"Saved UTC: {audit['saved_utc']}  ",
             f"Board: [Loom Task Board]({audit['project_url']})  ",
             f"Branch: {audit['branch']}", '',
             f"All {len(audit['cards'])} saved cards have fresh issue bodies/comments and checked local evidence paths.",
             'Current C++/firmware candidates remain uncompiled. No remote cards were moved or closed.', '',
             'Membership/columns come from the earlier board capture at ' + audit['membership_captured_utc'] + '.',
             'Discussions were read again at ' + audit['discussions_captured_utc'] + '.',
             'This audits that saved membership; it does not claim the board has no newly added cards.', '',
             'The JSON pairs each reviewed requirement with file hashes, comment URLs and remaining work.',
             'File hashes identify these candidates; they do not prove correctness or replace physical/live acceptance.', '',
             '## Gaps closed in this audit', '',
             '- #343: optional Manager health observer for pre-initialize output/completed calls; paced automatic flash attempts remain off by default. Flash example now actually disables SD.',
             '- #220: optional fresh Location fields appear from the first CSV row and keep their schema when fixes expire. GNSS/SD remain separately optional.',
             '- #314: optional repeated CSV identity columns can be hidden while JSON/upload identity stays intact.',
             '- #231/#251: read-only push/PR/manual host/source/mock workflow prepared, without dispatching it or running local compilers.',
             '- Sample demos report SD save failures; historical compile wording and profile counts corrected.', '',
             '## Remaining work that cannot be called complete', '',
             '- #303/#267/#273: slim hierarchy/package design, framework-wide pooling and unspecified Dendrometer/Evaporometer standby contracts.',
             '- #343: occasional flash checkpoints cover the bounded source option; durable per-operation crash tracking needs a backend/endurance design.',
             '- #355/#249/#257: actual meeting/training recordings, Teams/wiki publication and board release inputs/actions.',
             '- #291 and all changed C++: compilation is paused by the user. New CI is prepared locally and has no remote acceptance yet.',
             '- Hardware/live-service cards keep their explicit acceptance limits below. Offline mocks cannot establish production delivery.', '',
             '## Every saved card', '',
             '| Card | Local disposition | Addressed and evidence | Remaining |',
             '| --- | --- | --- | --- |']
    for row in audit['cards']:
        links = ', '.join(f'[{Path(path).name}](../../{path.replace(" ", "%20")})'
                          for path in row['evidence_sha256'])
        cells = [f"[#{row['id']}: {row['title']}]({row['url']})", row['disposition'].replace('_', ' '),
                 row['addressed'] + ' ' + links, row['remaining']]
        lines.append('| ' + ' | '.join(cell.replace('|', '\\|').replace('\n', ' ') for cell in cells) + ' |')
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('membership', 'inputs', 'definitions', 'output', 'report'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    # Never write an output selected outside this checkout.
    for path in (args.output, args.report):
        if not path.resolve().is_relative_to(ROOT):
            raise ValueError('audit outputs must stay inside this Loom checkout')
    audit = build_audit(read_json(args.membership), read_json(args.inputs), read_json(args.definitions))
    args.output.write_text(json.dumps(audit, indent=2) + '\n', encoding='utf-8')
    args.report.write_text(markdown(audit), encoding='utf-8')
    print(f"PASS: {len(audit['cards'])} cards, complete fresh comment counts and existing hashed evidence.")
    print(json.dumps(audit['counts'], sort_keys=True))


if __name__ == '__main__':
    main()
