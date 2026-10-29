#!/usr/bin/env python3
from campaign_output import prepare_campaign
from evidence import source_hashes
from report_sections import update_section
import csv,io,json,pathlib,subprocess,statistics,collections,random,hashlib
root=prepare_campaign('docs/benchmarks/sync')
meta={'campaigns':5,'warmup_steps':60,'measured_steps':600,'seed':42,'source_sha256':source_hashes()}
(root/'metadata.json').write_text(json.dumps(meta,indent=2))
groups=collections.defaultdict(list)
with (root/'raw.csv').open('w') as f:
 writer=csv.writer(f);writer.writerow(['campaign','backend','dataset','n','repeat','wall_ns','gpu_ns','checksum'])
 for campaign in range(5):
  for n in (100,300,600):
   outputs={};backends=['simulation','simulation_fullstate'];random.Random(campaign+n).shuffle(backends)
   for backend in backends:
    raw=subprocess.check_output(['build/perf/Release/quack_bench',str(n),'600','mixed',backend],text=True)
    rows=list(csv.DictReader(io.StringIO(raw)));writer.writerows([campaign]+list(row.values()) for row in rows);f.flush()
    outputs[backend]=[(r['dataset'],r['checksum']) for r in rows if r['backend']==backend]
    for name in (backend,backend+'_sync'):
     groups[name,n].append(statistics.median(float(r['wall_ns']) for r in rows if r['backend']==name))
   assert outputs['simulation']==outputs['simulation_fullstate'], 'Trajectory checksum/occupancy differs'
text='''\n## Vòng 3: tối ưu sync trên simulation thật\n
`Simulation::Update()` trước đây đọc full body state chỉ để lấy position/orientation khi sync sau physics: gồm cả velocity và center of mass không dùng. Thêm `ReadPose()` chỉ đọc position/rotation; giữ switch `pose_only_sync` để A/B. Pose-only là mặc định trong game. Không bỏ qua static body vì orientation của prop vẫn cần đồng bộ sau khởi tạo.\n
Năm campaign process riêng, 100/300/600 object, 60 warmup và 600 step đo; đảo thứ tự baseline/optimized theo seed. Raw log ghi placed và active rigid bodies từng step. Mọi cặp phải khớp chính xác chuỗi checksum vị trí và occupancy; checksum không chứng minh mọi component bằng nhau nên test trực tiếp cũng đối chiếu position/quaternion ReadPose với ReadState.\n
| Backend | N | Median campaign-median (ms) | Min–max (ms) |\n|---|---:|---:|---:|\n'''
for (b,n),values in sorted(groups.items()):text+=f'| {b} | {n} | {statistics.median(values)/1e6:.4f} | {min(values)/1e6:.4f}–{max(values)/1e6:.4f} |\n'
text+='\nTỷ lệ cải thiện phase sync và tổng update là hai số khác nhau:\n\n'
for n in (100,300,600):
 speed=statistics.median(groups['simulation_fullstate_sync',n])/statistics.median(groups['simulation_sync',n]);total=statistics.median(groups['simulation_fullstate',n])/statistics.median(groups['simulation',n]);text+=f'- {n} object: sync {speed:.2f}×; tổng update {total:.3f}×.\n'
text+='\nTái lập: `python3 scripts/benchmark_sync.py`. Kết quả ở `sync/`. Tổng update có thể nằm trong nhiễu do physics chi phối; không suy rộng speedup của sync thành FPS. Active-body telemetry và checksum nằm ngoài vùng đo tổng update. Đây là tối ưu API/read-path và giảm công việc thừa trong game, khác với kernel fixture độc lập.\n'
update_section(text,"\n## Vòng 3:")
(root/'checks.txt').write_text('PASS: 15 paired trajectories, 600 measured steps each; exact position checksum + placed/active occupancy match.\n')
print(text[-850:])
