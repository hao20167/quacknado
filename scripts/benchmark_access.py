#!/usr/bin/env python3
from campaign_output import prepare_campaign
from evidence import source_hashes
from report_sections import update_section
import collections,csv,hashlib,io,json,pathlib,random,statistics,subprocess
root=prepare_campaign('docs/benchmarks/access')
(root/'metadata.json').write_text(json.dumps({'campaigns':5,'steps':600,'warmup':60,'seed':42,'sha256':source_hashes()},indent=2))
groups=collections.defaultdict(list)
with (root/'raw.csv').open('w') as f:
 writer=csv.writer(f);writer.writerow(['campaign','backend','dataset','n','repeat','wall_ns','gpu_ns','checksum'])
 for campaign in range(5):
  for n in (100,600,1000):
   backends=['simulation_access_old','simulation_read_only','simulation'];random.Random(campaign+n).shuffle(backends);outputs={}
   for b in backends:
    rows=list(csv.DictReader(io.StringIO(subprocess.check_output(['build/perf/Release/quack_bench',str(n),'600','mixed',b],text=True))))
    writer.writerows([campaign]+list(r.values()) for r in rows);f.flush()
    outputs[b]=[(r['dataset'],r['checksum']) for r in rows if r['backend']==b]
    for phase in ('','_wind'):groups[b+phase,n].append(statistics.median(float(r['wall_ns']) for r in rows if r['backend']==b+phase))
   assert all(v==outputs['simulation'] for v in outputs.values()),'Trajectory checksum/occupancy mismatch'
text="\n## Vòng 5: giảm lock trên đường truy cập Jolt\n\nReadState gộp ba lần lấy state thành một BodyLockRead, tạo snapshot đồng bộ. ApplyForceAt bỏ GetMotionType riêng vì Jolt AddForce đã kiểm tra IsDynamic dưới write lock và thực hiện activation đúng semantics. Ba backend là old (3 reads + motion check), read_only (1 read + motion check), optimized (1 read + không check thừa). Không dùng NoLock hay gọi Jolt từ worker.\n\nTest trajectory so chính xác position, COM, rotation và linear/angular velocity với old path trong 120 step đổi hướng; static prop không nhận force vẫn được kiểm tra. Năm campaign độc lập, thứ tự backend shuffle, 60 warmup + 600 step, N=100/600/1000. Từng cặp khớp checksum vị trí và placed/active occupancy.\n\n| Backend | N | Median (ms) | Khoảng campaign (ms) |\n|---|---:|---:|---:|\n"
with (root/'summary.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['backend','n','median_ns','min_ns','max_ns'])
 for (b,n),v in sorted(groups.items()):
  w.writerow([b,n,statistics.median(v),min(v),max(v)])
  text+=f'| {b} | {n} | {statistics.median(v)/1e6:.4f} | {min(v)/1e6:.4f}–{max(v)/1e6:.4f} |\n'
for n in (100,600,1000):
 old=statistics.median(groups['simulation_access_old',n]);new=statistics.median(groups['simulation',n]);wind_old=statistics.median(groups['simulation_access_old_wind',n]);wind_new=statistics.median(groups['simulation_wind',n]);text+=f'\n- N={n}: tổng update {old/new:.3f}×, wind phase {wind_old/wind_new:.3f}×.\n'
text+='\nGiữ raw/metadata riêng trong access/. Đo toàn Update có physics, lock, sync, respawn; không ngoại suy thành FPS. ReadState mới fail-fast với invalid BodyID, các caller simulation lọc invalid trước khi đọc. Tái lập: python3 scripts/benchmark_access.py.\n'
update_section(text,"\n## Vòng 5:")
(root/'checks.txt').write_text('PASS: all paired per-step checksums and occupancy match.\n')
print(text[-1100:])
