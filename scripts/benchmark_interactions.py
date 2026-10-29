#!/usr/bin/env python3
from campaign_output import prepare_campaign
from evidence import source_hashes
import collections,csv,io,json,pathlib,random,statistics,subprocess
root=prepare_campaign('docs/benchmarks/interactions/campaign')
(root/'metadata.json').write_text(json.dumps({'source_sha256':source_hashes(),'seed':42,'warmup':60,'steps':300,'campaigns':5,'description':'Collision-off is diagnosis only; contact telemetry separate from timing.'},indent=2))
groups=collections.defaultdict(list);states={}
jobs=[(n,workers,collision) for n in (25,100,300,600,1000) for workers,collision in ((0,1),(0,0),(1,1),(2,1),(4,1))]
with (root/'raw.csv').open('w') as f:
 writer=None
 for campaign in range(5):
  random.Random(42+campaign).shuffle(jobs)
  for n,workers,collision in jobs:
   p=subprocess.run(['build/perf/Release/interaction_bench',str(n),str(workers),str(collision),'0','0'],capture_output=True,text=True,check=True)
   rows=list(csv.DictReader(io.StringIO(p.stdout)));assert len(rows)==300
   if writer is None:writer=csv.writer(f);writer.writerow(['campaign']+list(rows[0]))
   writer.writerows([campaign]+list(r.values()) for r in rows);f.flush()
   groups[n,workers,collision].append(statistics.median(float(r['physics_ns']) for r in rows))
   if collision:states[campaign,n,workers]=[(r['checksum'],r['active']) for r in rows]
with (root/'summary.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['n','workers','collision','median_physics_ns','min_campaign_ns','max_campaign_ns','exact_trajectory_vs_serial'])
 for (n,workers,collision),values in sorted(groups.items()):
  exact=all(states[c,n,workers]==states[c,n,0] for c in range(5)) if collision else 'diagnosis_changes_semantics'
  w.writerow([n,workers,collision,statistics.median(values),min(values),max(values),exact])
for collision in (0,1):
 p=subprocess.run(['build/perf/Release/interaction_bench','600','0',str(collision),'1','0'],capture_output=True,text=True,check=True)
 (root/f'contacts-collision-{collision}.csv').write_text(p.stdout)
print((root/'summary.csv').read_text())
