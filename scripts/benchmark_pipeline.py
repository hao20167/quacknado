#!/usr/bin/env python3
from campaign_output import prepare_campaign
from evidence import source_hashes
from report_sections import update_section
import collections,csv,hashlib,io,json,pathlib,random,statistics,subprocess
root=prepare_campaign('docs/benchmarks/pipeline')
(root/'metadata.json').write_text(json.dumps({'campaigns':5,'steps':600,'warmup':60,'seed':42,'sha256':source_hashes()},indent=2))
groups=collections.defaultdict(list)
with (root/'raw.csv').open('w') as f:
 writer=csv.writer(f);writer.writerow(['campaign','backend','dataset','n','repeat','wall_ns','gpu_ns','checksum'])
 for campaign in range(5):
  for n in (100,600,1000):
   backends=['simulation','simulation_pool1','simulation_pool2','simulation_pool4'];random.Random(campaign+n).shuffle(backends);outputs={}
   for b in backends:
    rows=list(csv.DictReader(io.StringIO(subprocess.check_output(['build/perf/Release/quack_bench',str(n),'600','mixed',b],text=True))))
    writer.writerows([campaign]+list(r.values()) for r in rows);f.flush()
    outputs[b]=[(r['dataset'],r['checksum']) for r in rows if r['backend']==b]
    for phase in ('','_wind'):groups[b+phase,n].append(statistics.median(float(r['wall_ns']) for r in rows if r['backend']==b+phase))
   assert all(v==outputs['simulation'] for v in outputs.values()),'Trajectory checksum/occupancy mismatch'
text='''\n## Vòng 4: worker pool trong wind pipeline thật\n
Đưa worker pool vào `src/core`, hỗ trợ truyền exception về caller và tái sử dụng pool sau lỗi. Simulation có backend tùy chọn 1/2/4 worker: main đọc BodyState → worker tính bốn điểm lực → main apply theo nguyên thứ tự → Jolt Step. Worker không gọi Jolt. Buffer reserve theo số object khi chọn backend; không tạo thread mỗi frame. Chế độ trực tiếp vẫn là mặc định cho đến khi end-to-end chứng minh lợi ích.\n
Correctness mạnh hơn checksum: test chạy tuần tự bốn Simulation cùng seed (tránh Jolt global factory đồng thời), 120 step với tornado đổi hướng; so chính xác position, COM, quaternion, linear/angular velocity từng body/step cho 0/1/2/4 worker. Test pool còn kiểm tra exception propagation và reuse sau lỗi.\n
Năm campaign độc lập, 60 warmup + 600 measured steps, thứ tự backend shuffle. So cả wind phase và tổng update; snapshot, dispatch, wait và apply nằm trong vùng đo. Raw giữ occupancy và checksum, mọi backend phải khớp direct.\n
| Backend | Requested objects | Median campaign-median (ms) | Min–max (ms) |\n|---|---:|---:|---:|\n'''
with (root/'summary.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['backend','n','median_ns','min_ns','max_ns'])
 for (b,n),v in sorted(groups.items()):
  w.writerow([b,n,statistics.median(v),min(v),max(v)])
  text+=f'| {b} | {n} | {statistics.median(v)/1e6:.4f} | {min(v)/1e6:.4f}–{max(v)/1e6:.4f} |\n'
text+='\nEnd-to-end speedup so với direct:\n\n'
for n in (100,600,1000):
 baseline=statistics.median(groups['simulation',n])
 text+=f'- N={n}: '+', '.join(f'{p} worker: {baseline/statistics.median(groups[f"simulation_pool{p}",n]):.3f}×' for p in (1,2,4))+'.\n'
text+='''
Không cộng speedup microkernel vào physics: các lock/read/apply và Jolt step vẫn tuần tự. 1.000 requested object không đảm bảo 1.000 body active; raw ghi placed/active. Worker pool không hỗ trợ nested run hoặc nhiều caller đồng thời; ownership thuộc Simulation/main thread. Exact equality đã kiểm tra cho fixture nêu trên, không phải chứng minh mọi scene/CPU. Đây là experiment tích hợp thật, giữ direct mặc định nếu các variant không thắng ổn định. Tái lập: `python3 scripts/benchmark_pipeline.py` sau khi build Release.\n'''
update_section(text,"\n## Vòng 4:")
(root/'checks.txt').write_text('PASS: 15 configurations x 4 backends; per-step position checksum and occupancy match.\n')
print(text[-1400:])
