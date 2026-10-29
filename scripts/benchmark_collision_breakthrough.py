#!/usr/bin/env python3
"""Compare real-clock catch-up and fixed-tick physics at the actual game scene count."""
import argparse
import collections
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import random
import statistics
import subprocess
import tarfile
from campaign_output import campaign_directory
from evidence import source_files, source_hashes

parser = argparse.ArgumentParser()
parser.add_argument('--output', required=True)
parser.add_argument('--before', default='build/collision-before/quacknado')
parser.add_argument('--after', default='build/perf/Release/quacknado')
parser.add_argument('--frames', type=int, default=360)
parser.add_argument('--campaigns', type=int, default=3)
args = parser.parse_args()
if not 120 <= args.frames <= 2000 or not 1 <= args.campaigns <= 10:
    parser.error('frames 120..2000; campaigns 1..10')
try:
    root = campaign_directory(args.output)
except ValueError as error:
    parser.error(str(error))
binaries = {'before': args.before, 'after': args.after}
metadata = {'source_sha256': source_hashes(), 'seed': 42, 'n': 1000, 'workers': 1,
            'warmup_frames': 60, 'frames': args.frames, 'campaigns': args.campaigns,
            'binaries': {k: {'path': v, 'sha256': hashlib.sha256(Path(v).read_bytes()).hexdigest()}
                         for k, v in binaries.items()},
            'description': 'Fixed tick comparisons use identical ticks; realtime comparisons share '
                           'the game catch-up policy and may reach different simulation ticks.'}
(root / 'metadata.json').write_text(json.dumps(metadata, indent=2))
with tarfile.open(root / 'measured-source.tar.gz', 'w:gz') as archive:
    for path in source_files():
        archive.add(path, arcname=path.as_posix())
groups = collections.defaultdict(list)
metrics = ('cpu_frame_ns', 'simulation_ns', 'physics_ns', 'wind_ns', 'gpu_command_ns')
per_process = []
with (root / 'raw.csv').open('w') as stream:
    writer = None
    for campaign in range(args.campaigns):
        jobs = [(label, clock, wind) for label in binaries for clock, wind in
                [('fixed', 1), ('realtime', 1), ('realtime', 0)]]
        random.Random(7300 + campaign).shuffle(jobs)
        fixed_states = {}
        for label, clock, wind in jobs:
            env = os.environ | {'QUACK_REPLAY_FRAMES': str(args.frames),
                                'QUACK_REPLAY_MODE': 'instanced', 'QUACK_SCENE_COUNT': '1000',
                                'QUACK_PHYSICS_WORKERS': '1', 'QUACK_PHYSICS_QOS': '0',
                                'QUACK_REPLAY_CLOCK': clock, 'QUACK_REPLAY_WIND': str(wind)}
            run = subprocess.run([binaries[label]], env=env, capture_output=True,
                                 text=True, timeout=180)
            tag = f'{campaign}-{label}-{clock}-{wind}'
            (root / f'{tag}.log').write_text(run.stderr)
            if run.returncode:
                (root / f'{tag}-failed.csv').write_text(run.stdout)
                run.check_returncode()
            rows = list(csv.DictReader(io.StringIO(run.stdout)))
            assert len(rows) == args.frames
            assert all(int(r['frame']) == i and int(r['scene_count']) == 1000 and
                       int(r['physics_workers']) == 1 and int(r['physics_qos']) == 0 and
                       r['clock'] == clock and int(r['wind_enabled']) == wind
                       for i, r in enumerate(rows))
            assert all(int(r['total_steps']) == sum(int(x['steps']) for x in rows[:i+1])
                       for i, r in enumerate(rows))
            if clock == 'fixed':
                assert all(int(r['steps']) == 1 for r in rows)
                fixed_states[label] = [(r['checksum'], r['active'], r['width'], r['height'])
                                       for r in rows]
            if writer is None:
                writer = csv.writer(stream)
                writer.writerow(['campaign', 'build'] + list(rows[0]))
            writer.writerows([campaign, label] + list(r.values()) for r in rows)
            stream.flush()
            measured = rows[60:]
            key = label, clock, wind
            info = {'campaign': campaign, 'build': label, 'clock': clock, 'wind': wind,
                    'total_steps': int(rows[-1]['total_steps']),
                    'measured_mean_steps_per_frame': statistics.mean(int(r['steps']) for r in measured),
                    'measured_frames_over_16_67ms': sum(float(r['cpu_frame_ns']) > 1e9/60 for r in measured),
                    'measured_elapsed_clamped_s': sum(max(0, float(r['elapsed_s']) - .1) for r in measured),
                    'final_active': int(rows[-1]['active'])}
            for metric in metrics:
                values = sorted(float(r[metric]) for r in measured)
                info[metric + '_median'] = statistics.median(values)
                info[metric + '_p99'] = values[int(.99 * (len(values) - 1))]
            per_process.append(info)
            groups[key].append(info)
            print(tag, 'frame median ms', round(info['cpu_frame_ns_median']/1e6, 3),
                  'p99', round(info['cpu_frame_ns_p99']/1e6, 3),
                  'mean steps', round(info['measured_mean_steps_per_frame'], 3), flush=True)
        # Different compiler optimization levels can diverge in chaotic contacts. Record
        # this explicitly; correctness requires independent physical invariants in both builds.
        exact = fixed_states['before'] == fixed_states['after']
        (root / f'fixed-state-{campaign}.json').write_text(json.dumps({'exact_checksum_active_resolution': exact}))
        if not exact:
            print('NOTE: fixed-tick checksums differ across build modes; no bitwise-fidelity claim',
                  flush=True)
with (root / 'per-process.csv').open('w') as stream:
    writer = csv.DictWriter(stream, fieldnames=list(per_process[0]))
    writer.writeheader()
    writer.writerows(per_process)
with (root / 'summary.csv').open('w') as stream:
    writer = csv.writer(stream)
    writer.writerow(['build', 'clock', 'wind', 'metric', 'median_process_value'])
    for (label, clock, wind), rows in sorted(groups.items()):
        for metric in list(rows[0])[4:]:
            writer.writerow([label, clock, wind, metric, statistics.median(r[metric] for r in rows)])
print((root / 'summary.csv').read_text())
