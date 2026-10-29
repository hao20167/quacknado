#!/usr/bin/env python3
"""Regenerate derived diagnostics from an immutable QoS campaign raw.csv."""
import argparse
import collections
import csv
import statistics
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('campaign', type=Path)
args = parser.parse_args()
groups = collections.defaultdict(list)
with (args.campaign / 'raw.csv').open() as stream:
    for row in csv.DictReader(stream):
        if int(row['frame']) >= 60:
            groups[int(row['campaign']), int(row['physics_workers']), int(row['physics_qos'])].append(row)
if not groups or any(len(rows) != 840 for rows in groups.values()):
    raise ValueError('Incomplete 900-frame campaign')
metrics = ('simulation_ns', 'physics_ns', 'wind_ns', 'cpu_frame_ns', 'gpu_command_ns')
with (args.campaign / 'per-process.csv').open('w') as stream:
    writer = csv.writer(stream)
    writer.writerow(['campaign', 'workers', 'qos', 'metric', 'median_ns', 'p99_ns', 'max_ns'])
    for key, rows in sorted(groups.items()):
        for metric in metrics:
            values = sorted(float(row[metric]) for row in rows)
            writer.writerow([*key, metric, statistics.median(values),
                             values[int(.99 * (len(values) - 1))], values[-1]])
fractions = []
with (args.campaign / 'slow-frames.csv').open('w') as stream:
    writer = csv.writer(stream)
    writer.writerow(['campaign', 'workers', 'qos', 'frame', 'simulation_ns',
                     'physics_ns', 'wind_ns', 'physics_fraction'])
    for key, rows in sorted(groups.items()):
        for row in sorted(rows, key=lambda r: float(r['simulation_ns']), reverse=True)[:10]:
            fraction = float(row['physics_ns']) / float(row['simulation_ns'])
            fractions.append(fraction)
            writer.writerow([*key, row['frame'], row['simulation_ns'], row['physics_ns'],
                             row['wind_ns'], fraction])
print(f'{len(groups)} processes; median physics fraction in top-10 simulation frames/process: '
      f'{statistics.median(fractions):.3f}')
