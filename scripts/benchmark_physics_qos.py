#!/usr/bin/env python3
"""Paired windowed experiment; QoS changes worker scheduling only."""
import argparse
import collections
import csv
import io
import json
import os
import random
import statistics
import subprocess
from campaign_output import campaign_directory
from evidence import source_hashes

parser = argparse.ArgumentParser()
parser.add_argument('--output', default='docs/benchmarks/interactions/qos-windowed')
parser.add_argument('--configs', default='0:0,2:0,2:1',
                    help='Comma-separated workers:qos pairs; must include 0:0 control')
args = parser.parse_args()
try:
    configs = [tuple(map(int, item.split(':'))) for item in args.configs.split(',')]
    if (any(len(c) != 2 for c in configs) or len(set(configs)) != len(configs)
            or (0, 0) not in configs or any(not 0 <= w <= 16 or q not in (0, 1)
                                           or (q and not w) for w, q in configs)):
        raise ValueError('Invalid config list (workers 0..16, qos 0/1; QoS requires workers)')
    root = campaign_directory(args.output)
except ValueError as error:
    parser.error(str(error))
(root / 'metadata.json').write_text(json.dumps({
    'source_sha256': source_hashes(), 'campaigns': 5, 'frames': 900,
    'warmup': 60, 'seed': 42, 'scene_count': 600, 'configs': configs,
    'renderer': 'instanced', 'phase_clocks': True,
    'description': 'Worker QoS user-initiated vs inherited; collision and solver unchanged.'
}, indent=2))
groups = collections.defaultdict(list)
with (root / 'raw.csv').open('w') as stream:
    writer = None
    for campaign in range(5):
        order = configs.copy()
        random.Random(4200 + campaign).shuffle(order)
        states = {}
        for workers, qos in order:
            env = os.environ | {
                'QUACK_REPLAY_FRAMES': '900', 'QUACK_REPLAY_MODE': 'instanced',
                'QUACK_SCENE_COUNT': '600', 'QUACK_REPLAY_CLOCK': 'fixed', 'QUACK_REPLAY_WIND': '1',
                'QUACK_PHYSICS_WORKERS': str(workers), 'QUACK_PHYSICS_QOS': str(qos)}
            result = subprocess.run(['build/perf/Release/quacknado'], env=env,
                                    capture_output=True, text=True, timeout=90)
            (root / f'{campaign}-{workers}-{qos}.log').write_text(result.stderr)
            if result.returncode:
                (root / f'{campaign}-{workers}-{qos}-failed.csv').write_text(result.stdout)
                result.check_returncode()
            rows = list(csv.DictReader(io.StringIO(result.stdout)))
            assert len(rows) == 900
            assert all(int(r['physics_workers']) == workers and int(r['physics_qos']) == qos
                       and int(r['frame']) == i for i, r in enumerate(rows))
            states[workers, qos] = [(r['checksum'], r['active'], r['width'], r['height']) for r in rows]
            if writer is None:
                writer = csv.writer(stream)
                writer.writerow(['campaign'] + list(rows[0]))
            writer.writerows([campaign] + list(r.values()) for r in rows)
            stream.flush()
            for metric in ('simulation_ns', 'physics_ns', 'wind_ns', 'cpu_frame_ns',
                           'submission_ns', 'gpu_command_ns'):
                values = sorted(float(r[metric]) for r in rows[60:])
                groups[workers, qos, metric].append((statistics.median(values),
                                                   values[int(.99 * (len(values) - 1))]))
            print(f'finished campaign={campaign} workers={workers} qos={qos}', flush=True)
        assert all(v == states[0, 0] for v in states.values()), 'Replay state/resolution mismatch'
with (root / 'summary.csv').open('w') as stream:
    writer = csv.writer(stream)
    writer.writerow(['workers', 'qos', 'metric', 'median_ns', 'median_campaign_p99_ns'])
    for (workers, qos, metric), values in sorted(groups.items()):
        writer.writerow([workers, qos, metric, statistics.median(v[0] for v in values),
                         statistics.median(v[1] for v in values)])
print((root / 'summary.csv').read_text())
