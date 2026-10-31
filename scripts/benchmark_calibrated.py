#!/usr/bin/env python3
from evidence import source_hashes
from report_sections import update_section
import argparse,platform
import collections,csv,hashlib,io,json,math,pathlib,random,statistics,subprocess
parser=argparse.ArgumentParser()
parser.add_argument('--output',default='docs/benchmarks/calibrated')
parser.add_argument('--no-report',action='store_true')
args=parser.parse_args()
root=pathlib.Path(args.output)
if root.exists() and any(root.iterdir()):raise SystemExit('Output directory is not empty; choose a new campaign path')
root.mkdir(parents=True,exist_ok=True)
meta={'campaigns':5,'repetitions':12,'calibration_target_ns':5000000,'bootstrap_seed':42,'bootstrap_resamples':10000,'sha256':source_hashes()}
meta['environment']={'os':platform.platform(),'arch':platform.machine(),'cpu':subprocess.check_output(['sysctl','-n','machdep.cpu.brand_string'],text=True).strip(),'compiler':subprocess.check_output(['clang++','--version'],text=True).strip(),'build_cache_sha256':hashlib.sha256(pathlib.Path('build/perf/CMakeCache.txt').read_bytes()).hexdigest()}
meta['binary_sha256']=hashlib.sha256(pathlib.Path('build/perf/Release/quack_bench').read_bytes()).hexdigest()
(root/'metadata.json').write_text(json.dumps(meta,indent=2))
jobs=[(n,d,b) for n in (1000,1000000) for d in ('inside','outside','mixed') for b in ('reference','aos','neon','neon_cull','pool4','pool_neon4')]
medians=collections.defaultdict(dict)
with (root/'raw.csv').open('w') as f,(root/'calibration.csv').open('w') as cf:
 w=csv.writer(f);w.writerow(['campaign','backend','dataset','n','repeat','wall_ns','gpu_ns','checksum'])
 cw=csv.writer(cf);cw.writerow(['campaign','backend','dataset','n','iterations_per_batch','calibration_ns'])
 for campaign in range(5):
  random.Random(42+campaign).shuffle(jobs)
  for n,d,b in jobs:
   p=subprocess.run(['build/perf/Release/quack_bench',str(n),'12',d,b,'--calibrated'],capture_output=True,text=True,check=True)
   rows=list(csv.DictReader(io.StringIO(p.stdout)))
   assert len(rows)==12 and all(r['backend']==b and int(r['n'])==n for r in rows)
   records=[line.split(',') for line in p.stderr.splitlines() if line.startswith('calibration,')];assert len(records)==1
   _,backend,size,iterations,elapsed=records[0];assert backend==b and int(size)==n and int(iterations)>0 and float(elapsed)>=5e6
   cw.writerow([campaign,b,d,n,iterations,elapsed]);cf.flush()
   w.writerows([campaign]+list(r.values()) for r in rows);f.flush()
   medians[b,d,n][campaign]=statistics.median(float(r['wall_ns']) for r in rows)
text='''\n## Vòng 10: calibrated batches và uncertainty của kernel benchmark\n
Thêm --calibrated cho một CPU kernel backend. Sau correctness/warmup, số invocation trong một batch tăng gấp đôi đến khi lượt calibration đạt ít nhất 5 ms (giới hạn 2^20, không đạt thì fail). Mỗi invocation có compiler memory barrier để tránh hoist/eliminate công việc lặp trên cùng input/output. Sau đó đo 12 batch với số invocation cố định và báo wall_ns/invocation; checksum nằm ngoài timer. Không đổi behavior default để giữ cách tái lập campaign cũ.\n
Mỗi cấu hình chạy năm process, thứ tự cấu hình shuffle theo campaign. Sweep inside/outside/mixed tại 1k và 1M sample. Lưu actual iterations và calibration duration riêng, không suy ra từ rounded timing. Các batch sau calibration có thể ngắn hơn 5ms nếu clock/cache thay đổi; target dùng chọn số lặp, không đảm bảo minimum mỗi repetition. Dataset được lặp lại hot-cache, không đại diện streaming workload mới mỗi invocation. Pool timing vẫn gồm dispatch/wait.\n
Speedup dùng geometric mean của năm tỷ lệ reference/variant theo campaign, mỗi tỷ lệ lấy process median. Khoảng 95% dùng percentile bootstrap resample năm campaign-ratio, 10.000 lần, seed 42. Với chỉ năm campaign đây là khoảng mô tả thăm dò, không đảm bảo coverage chuẩn hoặc kết luận significance. CV là độ lệch chuẩn population/mean của năm process median. Không gọi p99 của batch average là p99 invocation.\n
| Backend | Dataset | N | Median ns/invocation | CV process medians | Speedup vs reference | Bootstrap 95% interval |\n|---|---|---:|---:|---:|---:|---:|\n'''
with (root/'summary.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['backend','dataset','n','median_ns','process_cv','speedup_geomean','bootstrap_lo','bootstrap_hi'])
 for (b,d,n),samples in sorted(medians.items()):
  values=list(samples.values());ratios=[medians['reference',d,n][i]/samples[i] for i in range(5)]
  logs=[math.log(r) for r in ratios];ratio=math.exp(statistics.mean(logs));rng=random.Random(42)
  boot=sorted(math.exp(statistics.mean(rng.choices(logs,k=5))) for _ in range(10000));lo,hi=boot[249],boot[9749]
  cv=statistics.pstdev(values)/statistics.mean(values);median=statistics.median(values)
  w.writerow([b,d,n,median,cv,ratio,lo,hi])
  if d=='mixed' or (b=='neon_cull' and n==1000000):text+=f'| {b} | {d} | {n} | {median:.0f} | {cv:.1%} | {ratio:.2f}× | {lo:.2f}–{hi:.2f}× |\n'
text+='''\nKết quả mới không thay raw của vòng đầu. Không gộp kết quả calibrated với single-batch timing vì chính sách cache/measurement khác. Chưa pin affinity, khóa frequency hay đo thermal state; processor scheduling/P-E cores vẫn ảnh hưởng. Đây là cải thiện chất lượng phép đo, không phải claim kernel mới nhanh hơn do tăng batch. Correctness được chạy trước mỗi process measurement. CPU calibration không dùng cho simulation, render hoặc GPU; CLI từ chối thay vì báo thành công nhưng đo sai scope.\n
Tái lập: build quack_bench rồi `python3 scripts/benchmark_calibrated.py`. Log/metadata/raw/calibration records ở calibrated/.\n'''
if not args.no_report:update_section(text,'\n## Vòng 10:')
print(root)
