#!/usr/bin/env python3
"""Same 1000-box active contact pile, original libraries versus optimized libraries."""
import argparse
import collections
import csv
import hashlib
import io
import json
from pathlib import Path
import random
import statistics
import subprocess
from campaign_output import campaign_directory
from evidence import source_hashes

parser = argparse.ArgumentParser()
parser.add_argument('--output', default='docs/benchmarks/interactions/breakthrough/dense')
parser.add_argument('--before', default='build/collision-before/collision_stress_bench')
parser.add_argument('--after', default='build/perf/Release/collision_stress_bench')
args = parser.parse_args()
try:
    root = campaign_directory(args.output)
except ValueError as error:
    parser.error(str(error))
binaries = {'before': args.before, 'after': args.after}
proof = Path('docs/benchmarks/interactions/breakthrough/dense-before-build.json')
recorded_proof = None
if args.before == 'build/collision-before/collision_stress_bench' and proof.exists():
    (root / 'before-build-evidence.json').write_bytes(proof.read_bytes())
    recorded_proof = 'before-build-evidence.json'
(root / 'metadata.json').write_text(json.dumps({
    'source_sha256': source_hashes(), 'n': 1000, 'steps': 360, 'warmup': 60,
    'workers': 1, 'campaigns': 3, 'telemetry_in_timing': False,
    'binaries': {k: {'path': p, 'sha256': hashlib.sha256(Path(p).read_bytes()).hexdigest()}
                 for k, p in binaries.items()},
    'before_build_evidence': recorded_proof,
    'description': '10x10x10 boxes with binary-exact initial grid; alternating lateral '
                   'forcing keeps all bodies active. Per-step physical invariants in both builds.'
}, indent=2))
groups = collections.defaultdict(list)
comparison = []
with (root / 'raw.csv').open('w') as stream:
    writer = None
    for campaign in range(3):
        order = list(binaries)
        random.Random(9100 + campaign).shuffle(order)
        trajectories = {}
        for label in order:
            run = subprocess.run([binaries[label], '1', '0', '360'], capture_output=True,
                                 text=True, check=True, timeout=120)
            (root / f'{campaign}-{label}.log').write_text(run.stderr)
            if 'physics update failed' in run.stderr.lower():
                raise RuntimeError('Jolt capacity/update error invalidates benchmark')
            rows = list(csv.DictReader(io.StringIO(run.stdout)))
            assert len(rows) == 360 and all(int(r['active']) == 1000 for r in rows)
            assert [int(r['step']) for r in rows] == list(range(360))
            trajectories[label] = [(r['checksum'], r['active']) for r in rows]
            if writer is None:
                writer = csv.writer(stream)
                writer.writerow(['campaign', 'build'] + list(rows[0]))
            writer.writerows([campaign, label] + list(r.values()) for r in rows)
            stream.flush()
            values = sorted(float(r['physics_ns']) for r in rows[60:])
            result = (statistics.median(values), values[int(.99 * (len(values) - 1))])
            groups[label].append(result)
            print(campaign, label, 'median/p99 ms', result[0]/1e6, result[1]/1e6, flush=True)
        first = next((i for i, (a, b) in enumerate(zip(trajectories['before'], trajectories['after']))
                      if a != b), None)
        comparison.append({'campaign': campaign, 'exact_checksum_active': first is None,
                           'first_checksum_difference_step': first})
(root / 'state-comparison.json').write_text(json.dumps(comparison, indent=2))
with (root / 'summary.csv').open('w') as stream:
    writer = csv.writer(stream)
    writer.writerow(['build', 'median_physics_ns', 'median_campaign_p99_ns'])
    for label, values in sorted(groups.items()):
        writer.writerow([label, statistics.median(v[0] for v in values),
                         statistics.median(v[1] for v in values)])
run = subprocess.run([binaries['after'], '1', '1', '360'], capture_output=True,
                     text=True, check=True, timeout=120)
(root / 'contacts-diagnostic.csv').write_text(run.stdout)
(root / 'contacts-diagnostic.log').write_text(run.stderr)
if 'physics update failed' in run.stderr.lower():
    raise RuntimeError('Jolt capacity error in diagnostic')
print((root / 'summary.csv').read_text())
