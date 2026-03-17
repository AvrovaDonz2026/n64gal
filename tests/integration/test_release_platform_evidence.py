#!/usr/bin/env python3
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path('.').resolve()
SCRIPT = ['bash', 'scripts/release/run_platform_evidence.sh']


def main():
    out_dir = ROOT / 'tests' / 'integration' / 'release_platform_tmp'
    summary_md = out_dir / 'platform_evidence_summary.md'
    summary_json = out_dir / 'platform_evidence_summary.json'
    if out_dir.exists():
        import shutil
        shutil.rmtree(out_dir)

    help_proc = subprocess.run(
        SCRIPT + ['--help'],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if help_proc.returncode != 0:
        print(f'platform help failed rc={help_proc.returncode} stderr={help_proc.stderr}', file=sys.stderr)
        return 1
    if '--summary-json-out <path>' not in help_proc.stderr:
        print('platform help missing summary-json-out option', file=sys.stderr)
        return 1

    proc = subprocess.run(
        SCRIPT + ['--out-dir', str(out_dir), '--summary-out', str(summary_md), '--summary-json-out', str(summary_json)],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        print(f'release platform evidence failed rc={proc.returncode} stdout={proc.stdout} stderr={proc.stderr}', file=sys.stderr)
        return 1
    if 'trace_id=release.platform.ok' not in proc.stdout:
        print('missing release platform success trace', file=sys.stderr)
        return 1

    if not summary_md.exists() or not summary_json.exists():
        print('platform evidence summaries missing', file=sys.stderr)
        return 1

    summary_text = summary_md.read_text(encoding='utf-8')
    if '# Platform Evidence Summary' not in summary_text:
        print('platform summary header missing', file=sys.stderr)
        return 1
    if 'amd64' not in summary_text or 'arm64' not in summary_text:
        print('platform summary missing rows', file=sys.stderr)
        return 1
    if '`release.platform.ok`' not in summary_text:
        print('platform summary missing trace id', file=sys.stderr)
        return 1
    if str(summary_json) not in summary_text:
        print('platform summary missing summary json path', file=sys.stderr)
        return 1

    payload = json.loads(summary_json.read_text(encoding='utf-8'))
    if payload.get('trace_id') != 'release.platform.ok' or payload.get('status') != 'ok':
        print('platform json missing ok trace/status', file=sys.stderr)
        return 1
    if payload.get('summary_md') != str(summary_md) or payload.get('summary_json') != str(summary_json):
        print('platform json summary paths inconsistent', file=sys.stderr)
        return 1
    if not payload.get('ci_status'):
        print('platform json missing ci status', file=sys.stderr)
        return 1
    release_commands = payload.get('release_commands')
    if not isinstance(release_commands, list) or len(release_commands) != 4:
        print('platform json missing release commands', file=sys.stderr)
        return 1
    rows = payload.get('rows')
    if not isinstance(rows, list) or len(rows) != 5:
        print('platform json missing structured rows', file=sys.stderr)
        return 1
    first_row = rows[0]
    if first_row.get('arch') != 'amd64' or first_row.get('os') != 'Linux':
        print('platform json first row mismatch', file=sys.stderr)
        return 1
    if 'raw' not in first_row or 'backend_policy' not in first_row or 'artifacts' not in first_row:
        print('platform json row missing fields', file=sys.stderr)
        return 1

    import shutil
    shutil.rmtree(out_dir)
    print('test_release_platform_evidence ok')
    return 0


if __name__ == '__main__':
    sys.exit(main())
