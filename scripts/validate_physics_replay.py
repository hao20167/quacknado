#!/usr/bin/env python3
"""Validate default/serial/QoS replay under project sanitizers and Metal validation."""
import csv
import io
import json
import os
import subprocess
from campaign_output import prepare_campaign
from evidence import source_hashes

root = prepare_campaign('docs/benchmarks/interactions/final-smoke')
configs = {'default': {}, 'serial': {'QUACK_PHYSICS_WORKERS': '0'},
           'qos1': {'QUACK_PHYSICS_WORKERS': '1', 'QUACK_PHYSICS_QOS': '1'},
           'qos2': {'QUACK_PHYSICS_WORKERS': '2', 'QUACK_PHYSICS_QOS': '1'}}
(root / 'metadata.json').write_text(json.dumps({
    'source_sha256': source_hashes(), 'frames': 60, 'scene_count': 1000,
    'binary': 'build/check/Debug/quacknado', 'configs': configs,
    'vendor_instrumented': False, 'performance_data': False}, indent=2))
states = {}
workers = {}
with (root / 'raw.csv').open('w') as stream:
    writer = None
    for name, extra in configs.items():
        env = os.environ.copy()
        for key in ('QUACK_PHYSICS_WORKERS', 'QUACK_PHYSICS_QOS'):
            env.pop(key, None)
        env.update({'QUACK_REPLAY_FRAMES': '60', 'QUACK_REPLAY_MODE': 'instanced',
                    'QUACK_SCENE_COUNT': '1000', 'QUACK_REPLAY_CLOCK': 'fixed', 'QUACK_REPLAY_WIND': '1',
                    'ASAN_OPTIONS': 'halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1',
                    'MTL_DEBUG_LAYER': '1'})
        env.update(extra)
        result = subprocess.run(['build/check/Debug/quacknado'], env=env,
                                capture_output=True, text=True, timeout=90)
        (root / f'{name}.log').write_text(result.stderr)
        if result.returncode:
            (root / f'{name}-failed.csv').write_text(result.stdout)
            result.check_returncode()
        rows = list(csv.DictReader(io.StringIO(result.stdout)))
        assert len(rows) == 60
        assert all(int(row['frame']) == i for i, row in enumerate(rows))
        states[name] = [(r['checksum'], r['active'], r['width'], r['height']) for r in rows]
        workers[name] = sorted({int(r['physics_workers']) for r in rows})
        if name in ('qos1', 'qos2'):
            assert all(int(r['physics_qos']) == 1 for r in rows)
            assert workers[name] == [int(extra['QUACK_PHYSICS_WORKERS'])]
        elif name == 'serial':
            assert workers[name] == [0]
        else:
            assert len(workers[name]) == 1 and workers[name][0] in (0, 1)
        if writer is None:
            writer = csv.writer(stream)
            writer.writerow(['config'] + list(rows[0]))
        writer.writerows([name] + list(row.values()) for row in rows)
assert all(value == states['serial'] for value in states.values()), 'Replay state mismatch'
(root / 'summary.json').write_text(json.dumps({
    'exact_state_checksum_active_resolution': True, 'workers': workers,
    'frames_per_config': 60, 'sanitizer_or_validation_failure': False}, indent=2))
print((root / 'summary.json').read_text())
