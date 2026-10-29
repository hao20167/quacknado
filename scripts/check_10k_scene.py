#!/usr/bin/env python3
"""Check real physics population and the instanceCount arguments of completed Metal draws."""
import csv
import hashlib
import io
import json
import os
import re
from pathlib import Path
import subprocess
import tarfile
from campaign_output import prepare_campaign
from evidence import source_hashes, source_files

root = prepare_campaign('docs/benchmarks/spawn-10k/verified')
binary = Path('build/default/quacknado')
asset_hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in Path('assets/models').glob('*.glb')}
for path, expected in asset_hashes.items():
    assert hashlib.sha256((binary.parent / path).read_bytes()).hexdigest() == expected, 'Stale copied asset'
(root / 'metadata.json').write_text(json.dumps({
    'source_sha256': source_hashes(), 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
    'scene_count': 10000, 'frames': 360, 'asset_sha256': asset_hashes, 'clock': 'fixed', 'seed': 42, 'workers': 1,
    'timings_are_diagnostic_only': True,
    'counts': 'placed includes floor/props; model_objects excludes floor; instance invocations '
              'sum instanceCount over model primitive draw calls. Not a visible-pixel count.'
}, indent=2))
with tarfile.open(root / 'measured-source.tar.gz', 'w:gz') as archive:
    for path in source_files():
        archive.add(path, arcname=path.as_posix())
env = os.environ | {'QUACK_SCENE_COUNT': '10000', 'QUACK_REPLAY_FRAMES': '360',
                    'QUACK_REPLAY_MODE': 'instanced', 'QUACK_REPLAY_CLOCK': 'fixed',
                    'QUACK_REPLAY_WIND': '1', 'QUACK_PHYSICS_WORKERS': '1', 'QUACK_PHYSICS_QOS': '0',
                    'MTL_DEBUG_LAYER': '1'}
run = subprocess.run([str(binary)], env=env, text=True, capture_output=True, timeout=240)
(root / 'app.log').write_text(run.stderr)
(root / 'raw.csv').write_text(run.stdout)
run.check_returncode()
primitives = {name: int(count) for name, count in re.findall(r'Loaded .*/([^/]+)\.glb \((\d+) primitives\)', run.stderr)}
census = {name: int(count) for name, count in re.findall(r'ReplayModel name=(\S+) objects=(\d+)', run.stderr)}
assert sum(census.values()) == 10002 and set(census) <= set(primitives)
expected_instances = sum(count * primitives[name] for name, count in census.items())
(root / 'model-census.json').write_text(json.dumps({'objects': census, 'primitives_per_model': primitives, 'expected_instance_invocations': expected_instances}, indent=2))
rows = list(csv.DictReader(io.StringIO(run.stdout)))
assert len(rows) == 360
assert all(int(r['frame']) == i and int(r['total_steps']) == i+1 and r['scene_count'] == '10000'
           and r['clock'] == 'fixed' and r['placed'] == '10003' and r['pending'] == '0'
           and r['model_objects'] == '10002' and int(r['model_instance_invocations']) == expected_instances
           for i, r in enumerate(rows)), 'Missing spawn or model submission'
(root / 'summary.json').write_text(json.dumps({
    'frames_checked': 360, 'placed_every_frame': 10003, 'pending_every_frame': 0,
    'model_objects_every_frame': 10002, 'model_instance_invocations_every_frame': expected_instances,
    'respawns': sum(int(r['respawned']) for r in rows),
    'gpu_commands_completed_without_error': True,
    'note': 'FinishProfile waits/checks all command buffers before CSV is printed; '
            'camera clipping and occlusion can still hide pixels.'
}, indent=2))
for path, expected in asset_hashes.items():
    assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == expected, 'Assets changed during run'
print((root / 'summary.json').read_text())
