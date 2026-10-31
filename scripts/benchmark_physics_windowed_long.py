#!/usr/bin/env python3
from campaign_output import prepare_campaign
from evidence import source_hashes
import collections,csv,io,json,os,pathlib,random,statistics,subprocess
root=prepare_campaign('docs/benchmarks/interactions/windowed-long')
(root/'metadata.json').write_text(json.dumps({'source_sha256':source_hashes(),'campaigns':3,'frames':900,'warmup':60,'seed':42,'scene_count':600,'renderer':'instanced','workers':[0,2]},indent=2))
groups=collections.defaultdict(list)
with (root/'raw.csv').open('w') as f:
 writer=None
 for campaign in range(3):
  configs=[0,2];random.Random(42+campaign).shuffle(configs);states={}
  for workers in configs:
   env=os.environ|{'QUACK_REPLAY_FRAMES':'900','QUACK_REPLAY_MODE':'instanced','QUACK_SCENE_COUNT':'600','QUACK_REPLAY_CLOCK':'fixed','QUACK_REPLAY_WIND':'1','QUACK_PHYSICS_QOS':'0','QUACK_PHYSICS_WORKERS':str(workers)}
   p=subprocess.run(['build/perf/Release/quacknado'],env=env,text=True,capture_output=True,timeout=60)
   (root/f'{campaign}-{workers}.log').write_text(p.stderr)
   if p.returncode:(root/f'{campaign}-{workers}-failed.csv').write_text(p.stdout);p.check_returncode()
   rows=list(csv.DictReader(io.StringIO(p.stdout)));assert len(rows)==900
   assert all(int(r['physics_workers'])==workers and int(r['frame'])==i for i,r in enumerate(rows))
   states[workers]=[(r['checksum'],r['active'],r['width'],r['height']) for r in rows]
   if writer is None:writer=csv.writer(f);writer.writerow(['campaign']+list(rows[0]))
   writer.writerows([campaign]+list(r.values()) for r in rows);f.flush()
   for metric in ('simulation_ns','cpu_frame_ns','submission_ns','gpu_command_ns'):
    v=sorted(float(r[metric]) for r in rows[60:]);groups[workers,metric].append((statistics.median(v),v[int(.99*(len(v)-1))]))
  assert all(v==states[0] for v in states.values()),'Physics/resolution replay mismatch'
with (root/'summary.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['workers','metric','median_ns','median_campaign_p99_ns'])
 for (workers,metric),v in sorted(groups.items()):w.writerow([workers,metric,statistics.median(x[0] for x in v),statistics.median(x[1] for x in v)])
print((root/'summary.csv').read_text())
