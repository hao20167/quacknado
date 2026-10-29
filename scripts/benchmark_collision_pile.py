#!/usr/bin/env python3
"""Paired 10k no-wind CPU and real-clock game campaign; refuses to overwrite evidence."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import tarfile

from campaign_output import campaign_directory
from evidence import source_hashes


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def stats(rows, column):
    values = sorted(float(r[column]) / 1e6 for r in rows)
    return {'median_ms': statistics.median(values),
            'p99_ms': values[int(.99 * (len(values) - 1))]}


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--output', default='docs/benchmarks/collision-pile/paired')
    parser.add_argument('--before-game', default='build/collision-pile-before/quacknado')
    parser.add_argument('--after-game', default='build/default/quacknado')
    parser.add_argument('--before-cpu', default='docs/benchmarks/collision-pile/baseline-interaction-bench')
    parser.add_argument('--after-cpu', default='build/default/interaction_bench')
    parser.add_argument('--campaigns', type=int, default=3)
    parser.add_argument('--frames', type=int, default=600)
    args = parser.parse_args()
    if args.campaigns < 1 or not 60 < args.frames <= 2000:
        parser.error('campaigns >= 1 and frames 61..2000 required')
    try:
        root = campaign_directory(args.output)
    except ValueError as error:
        parser.error(str(error))
    binaries = {k: Path(getattr(args, k)).resolve() for k in
                ('before_game', 'after_game', 'before_cpu', 'after_cpu')}
    sources = source_hashes()
    cpu = subprocess.run(['sysctl', '-n', 'machdep.cpu.brand_string'],
                         capture_output=True, text=True, check=False)
    assets = {str(p): digest(p) for p in sorted(Path('assets/models').glob('*.glb'))}
    for label in ('before_game', 'after_game'):
        base = binaries[label].parent
        assert digest(base / 'object.metallib') == digest('build/default/object.metallib')
        for asset, sha in assets.items():
            assert digest(base / asset) == sha, f'Asset mismatch: {label}: {asset}'
    (root / 'metadata.json').write_text(json.dumps({
        'source_sha256': sources, 'asset_sha256': assets,
        'binary_sha256': {k: digest(v) for k, v in binaries.items()},
        'binary_paths': {k: str(v) for k, v in binaries.items()},
        'cpu': cpu.stdout.strip() or 'unavailable', 'hardware_threads': os.cpu_count(),
        'platform': platform.platform(),
        'count': 10000, 'seed': 42, 'cpu_steps': 1200, 'warmup_steps': 60,
        'frames': args.frames, 'frame_warmup': 60, 'campaigns': args.campaigns,
        'wind': False, 'game_clock': 'realtime', 'dt': '1/60',
        'before_workers': 1, 'after_workers': '6 CPU / automatic game policy',
        'telemetry_in_timing': False, 'metal_validation_in_timing': False,
        'note': 'Same solver/contact/dt/catch-up. Frame runs have equal frame count, '
                'not equal simulated duration; use CPU runs for matching trajectories.'
    }, indent=2) + '\n')
    with tarfile.open(root / 'measured-source.tar.gz', 'w:gz') as archive:
        for file in sources:
            archive.add(file)
    results = []
    for campaign in range(args.campaigns):
        for kind in ('cpu', 'game'):
            order = ['before', 'after']
            random.Random(29310 + 7 * campaign + (kind == 'game')).shuffle(order)
            trajectories = {}
            for label in order:
                command = [str(binaries[f'{label}_{kind}'])]
                env = os.environ.copy()
                for key in list(env):
                    if key.startswith('QUACK_') or key.startswith('MTL_'):
                        del env[key]
                if kind == 'cpu':
                    command += ['10000', '1' if label == 'before' else '6',
                                '1', '0', '0', '0', '0', '1200']
                else:
                    env.update(QUACK_SCENE_COUNT='10000', QUACK_REPLAY_FRAMES=str(args.frames),
                               QUACK_REPLAY_CLOCK='realtime', QUACK_REPLAY_WIND='0')
                    if label == 'before':
                        env['QUACK_PHYSICS_WORKERS'] = '1'
                stem = f'{campaign}-{kind}-{label}'
                (root / f'{stem}-command.json').write_text(json.dumps({
                    'argv': command, 'environment': {k: v for k, v in env.items()
                                                   if k.startswith('QUACK_')}
                }, indent=2) + '\n')
                with (root / f'{stem}.csv').open('w') as out, (root / f'{stem}.log').open('w') as err:
                    subprocess.run(command, env=env, stdout=out, stderr=err, check=True, timeout=300)
                rows = list(csv.DictReader((root / f'{stem}.csv').open()))
                assert len(rows) == (1200 if kind == 'cpu' else args.frames)
                assert all(int(r['placed']) == 10003 and int(r['pending']) == 0 for r in rows)
                if kind == 'cpu':
                    trajectories[label] = [(r['checksum'], r['active'], r['respawned']) for r in rows]
                    summary = stats(rows[60:], 'physics_ns')
                else:
                    assert all(int(r['physics_workers']) == (1 if label == 'before' else 6) for r in rows)
                    assert all(int(r['model_objects']) == 10002 for r in rows)
                    assert all(int(r['model_instance_invocations']) == 22258 for r in rows)
                    assert all(r['wind_enabled'] == '0' and r['clock'] == 'realtime' for r in rows)
                    summary = stats(rows[60:], 'cpu_frame_ns')
                    summary.update(frames_over_33ms=sum(float(r['cpu_frame_ns']) > 33.333333e6 for r in rows),
                                   six_step_frames=sum(int(r['steps']) == 6 for r in rows),
                                   max_steps=max(int(r['steps']) for r in rows),
                                   total_steps=int(rows[-1]['total_steps']),
                                   gpu=stats(rows[60:], 'gpu_command_ns'))
                result = {'campaign': campaign, 'kind': kind, 'build': label, **summary}
                results.append(result)
                (root / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
                print(json.dumps(result), flush=True)
            if kind == 'cpu':
                assert trajectories['before'] == trajectories['after'], 'CPU trajectory mismatch'
    assert source_hashes() == sources, 'Source changed during measurement'
    assert {str(p): digest(p) for p in sorted(Path('assets/models').glob('*.glb'))} == assets
    for key, value in binaries.items():
        assert digest(value) == json.loads((root / 'metadata.json').read_text())['binary_sha256'][key]
    (root / 'verification.json').write_text(json.dumps({
        'status': 'passed', 'cpu_trajectory_checksums_active_respawns_exact': True,
        'all_frames_population_and_model_counts': True,
        'gpu_commands_completed': True, 'sources_assets_binaries_unchanged': True
    }, indent=2) + '\n')


if __name__ == '__main__':
    main()
