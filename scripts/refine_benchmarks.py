#!/usr/bin/env python3
"""Independent-process campaign; preserves the original results."""
from campaign_output import prepare_campaign
from evidence import source_files
import csv,io,json,pathlib,random,subprocess,hashlib
root=prepare_campaign('docs/benchmarks/refinement')
files=source_files()
(root/'metadata.json').write_text(json.dumps({'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},'seed':42,'campaigns':5,'warmup':3,'repetitions':30,'note':'Jolt before/after share fixture; fresh world per cell size. Same backend order for spatial; randomized wind job order.'},indent=2))
jobs=[(n,d,b) for n in (1000,1000000) for d in ('inside','outside','mixed') for b in ('neon','neon_cull')]+[(n,d,'spatial') for n in (100,1000) for d in ('uniform','clustered')]
with (root/'raw.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['campaign','backend','dataset','n','repeat','wall_ns','gpu_ns','checksum'])
 for campaign in range(5):
  random.Random(campaign).shuffle(jobs)
  for n,d,b in jobs:
   text=subprocess.check_output(['build/perf/Release/quack_bench',str(n),'30',d,b],text=True)
   w.writerows([campaign]+row for row in list(csv.reader(io.StringIO(text)))[1:]);f.flush()
