#!/usr/bin/env python3
"""Conservative descriptive gate. Exit 0 pass, 1 regression, 2 inconclusive/invalid."""
import argparse,collections,csv,json,math,pathlib,random,statistics

def load(root):
    root=pathlib.Path(root)
    meta=json.loads((root/'metadata.json').read_text())
    if not meta.get('environment') or any(not v for v in meta['environment'].values()):
        raise ValueError('Missing environment identity')
    groups=collections.defaultdict(lambda:collections.defaultdict(list));seen=set()
    for r in csv.DictReader((root/'raw.csv').read_text().splitlines()):
        key=(r['backend'],r['dataset'],int(r['n']));campaign=int(r['campaign']);repeat=int(r['repeat'])
        if campaign not in range(meta['campaigns']) or repeat not in range(meta['repetitions']):raise ValueError('Out-of-range measurement identity')
        identity=(key,campaign,repeat)
        if identity in seen:raise ValueError('Duplicate measurement')
        seen.add(identity);value=float(r['wall_ns'])
        if not math.isfinite(value) or value<=0:raise ValueError('Invalid timing')
        groups[key][campaign].append(value)
    if not groups:raise ValueError('Empty campaign')
    medians={}
    for key,runs in groups.items():
        if len(runs)!=meta['campaigns'] or len(runs)<5:raise ValueError('Insufficient process samples')
        if any(len(v)!=meta['repetitions'] for v in runs.values()):raise ValueError('Incomplete repetition set')
        medians[key]=[statistics.median(v) for v in runs.values()]
    calibration_seen=set()
    for r in csv.DictReader((root/'calibration.csv').read_text().splitlines()):
        key=(r['backend'],r['dataset'],int(r['n']));identity=(key,int(r['campaign']))
        if key not in groups or identity[1] not in groups[key] or identity in calibration_seen:raise ValueError('Invalid calibration identity')
        calibration_seen.add(identity)
        elapsed=float(r['calibration_ns'])
        if int(r['iterations_per_batch'])<=0 or not math.isfinite(elapsed) or elapsed<meta['calibration_target_ns']:raise ValueError('Invalid calibration evidence')
    if len(calibration_seen)!=sum(len(runs) for runs in groups.values()):raise ValueError('Missing calibration evidence')
    return meta,medians

def compare(baseline,candidate,budget=.05,noise=.10):
    bm,b=load(baseline);cm,c=load(candidate)
    if bm['environment']!=cm['environment']:raise ValueError('Environment mismatch')
    for field in ('campaigns','repetitions','calibration_target_ns'):
        if bm.get(field)!=cm.get(field):raise ValueError('Measurement policy mismatch')
    if set(b)!=set(c):raise ValueError('Workload set mismatch')
    rows=[]
    for key in sorted(b):
        x,y=b[key],c[key];lx=list(map(math.log,x));ly=list(map(math.log,y));rng=random.Random(42)
        ratio=math.exp(statistics.mean(ly)-statistics.mean(lx))
        boot=sorted(math.exp(statistics.mean(rng.choices(ly,k=len(y)))-statistics.mean(rng.choices(lx,k=len(x)))) for _ in range(10000))
        lo,hi=boot[249],boot[9749];cv=max(statistics.pstdev(x)/statistics.mean(x),statistics.pstdev(y)/statistics.mean(y))
        status='REGRESSION' if lo>1+budget else 'INCONCLUSIVE' if cv>noise or hi>1+budget else 'PASS'
        rows.append(dict(backend=key[0],dataset=key[1],n=key[2],latency_ratio=ratio,lo=lo,hi=hi,max_cv=cv,status=status))
    status='REGRESSION' if any(r['status']=='REGRESSION' for r in rows) else 'INCONCLUSIVE' if any(r['status']=='INCONCLUSIVE' for r in rows) else 'PASS'
    return {'status':status,'budget':budget,'noise_limit':noise,'comparisons':rows,'scope':'Descriptive bootstrap on process medians; not calibrated familywise statistical significance.'}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('baseline');p.add_argument('candidate');p.add_argument('--output');a=p.parse_args()
    try:result=compare(a.baseline,a.candidate)
    except (ValueError,KeyError,OSError,json.JSONDecodeError) as e:result={'status':'INCONCLUSIVE','reason':str(e)}
    text=json.dumps(result,indent=2);print(text)
    if a.output:pathlib.Path(a.output).write_text(text+'\n')
    raise SystemExit({'PASS':0,'REGRESSION':1,'INCONCLUSIVE':2}[result['status']])
