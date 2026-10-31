#!/usr/bin/env python3
from evidence import source_hashes
"""Reproducible runner. No Python third-party dependencies required."""
from campaign_output import campaign_directory
import hashlib
import argparse, csv, io, json, pathlib, platform, random, subprocess, time
p=argparse.ArgumentParser()
p.add_argument('--binary',default='build/perf/Release/quack_bench')
p.add_argument('--output',default='docs/benchmarks/results')
p.add_argument('--repetitions',type=int,default=20)
p.add_argument('--cpu-only',action='store_true')
a=p.parse_args();out=campaign_directory(a.output)
def command(*args):return subprocess.check_output(args,text=True).strip()
metadata={'commit':command('git','rev-parse','HEAD'),'dirty':command('git','status','--short'),'platform':platform.platform(),'machine':command('sysctl','-n','machdep.cpu.brand_string'),'compiler':command('clang++','--version'),'time':time.strftime('%Y-%m-%dT%H:%M:%S%z'),'command':vars(a),'seed':42,'tolerance':'abs <= 2e-4 + 2e-5 * abs(reference)'}
metadata['source_sha256']=source_hashes()
binary_path=pathlib.Path(a.binary).resolve()
build_path=binary_path.parent.parent if binary_path.parent.name in ('Release','Debug','RelWithDebInfo','MinSizeRel') else binary_path.parent
metadata['binary_sha256']=hashlib.sha256(binary_path.read_bytes()).hexdigest()
metadata['compile_commands']=[x for x in json.loads((build_path/'compile_commands.json').read_text()) if '/external/' not in x['file']]
metadata['cmake_cache']=(build_path/'CMakeCache.txt').read_text()
(out/'metadata.json').write_text(json.dumps(metadata,indent=2))
jobs=[(n,d,b) for n in (1000,10000,100000,1000000) for d in ('mixed','inside','outside') for b in ('reference','aos','soa','neon','pool1','pool2','pool4','pool_neon4')+(() if a.cpu_only else ('gpu',))]
random.Random(42).shuffle(jobs)
jobs += ([] if a.cpu_only else [(n,'offscreen','render') for n in (1000,10000)])
jobs += [(n,'mixed',b) for n in (100,300,600) for b in ('simulation','simulation_reference')]
jobs += [(n,d,'spatial') for n in (100,600,1000) for d in ('uniform','clustered')]
with (out/'raw.csv').open('w') as f:
 writer=csv.DictWriter(f,fieldnames=['backend','dataset','n','repeat','wall_ns','gpu_ns','checksum']);writer.writeheader()
 for n,d,b in jobs:
  result=subprocess.run([a.binary,str(n),str(max(600,a.repetitions) if b.startswith('simulation') else a.repetitions),d,b],text=True,capture_output=True)
  if result.returncode:
   (out/'failure.txt').write_text(f'{n} {d} {b}\n'+result.stderr);raise RuntimeError(result.stderr)
  writer.writerows(csv.DictReader(io.StringIO(result.stdout)));f.flush()
print(out/'raw.csv')
