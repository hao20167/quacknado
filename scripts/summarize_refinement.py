#!/usr/bin/env python3
from report_sections import update_section
import csv,collections,pathlib,statistics,json
root=pathlib.Path('docs/benchmarks/refinement')
groups=collections.defaultdict(lambda:collections.defaultdict(list))
for r in csv.DictReader((root/'raw.csv').open()):groups[(r['backend'],r['dataset'],int(r['n']))][int(r['campaign'])].append(float(r['wall_ns']))
summary={k:[statistics.median(v) for v in runs.values()] for k,runs in groups.items()}
with (root/'summary.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['backend','dataset','n','campaigns','median_ns','min_campaign_ns','max_campaign_ns'])
 for k,v in sorted(summary.items()):w.writerow([*k,len(v),statistics.median(v),min(v),max(v)])
text='''\n## Vòng cải tiến 2: broadphase và SIMD culling (29/10/2026)

Giữ nguyên campaign đầu trong `results/`. Vòng mới ở `refinement/`: năm campaign, mỗi cấu hình chạy process riêng, warmup 3 lượt rồi 30 batch; báo median của năm campaign-median và khoảng min–max giữa campaign. Wind shuffle thứ tự job; spatial giữ thứ tự before → optimize → after. Lượt thu chính thức chạy sau khi build/sanitizer đã xong. Đây vẫn là một máy và chưa kiểm soát thermal/affinity.

**Jolt:** thêm `PhysicsWorld::OptimizeBroadPhase()` để thử rebuild sau bulk insertion tại điểm không có Step/query đồng thời. Mỗi cell-size fixture tạo PhysicsWorld mới; tránh tree đã optimize làm nhiễm baseline sau. Cả before/after đều kiểm tra từng query với linear reference. Chi phí optimize được đo riêng một lần mỗi process/fixture, tổng năm sample. Đây là phép thử cấu hình thư viện, không phải thuật toán broadphase tự viết. Không tự động gọi mỗi frame hoặc thay grid vào game.

**NEON:** thêm backend `neon_cull`, giữ `neon` làm control. Sau khi tính bán kính, dùng mask kiểm tra cả bốn lane ngoài vùng ảnh hưởng; nếu đúng thì ghi zero và bỏ các phép tính tiếp theo. Nhóm có lane còn hoạt động đi qua implementation cũ. Kiểm tra numerical tolerance như vòng đầu, test 10.003 sample cho phần dư. Chưa chọn culling làm mặc định vì overhead mask có thể bất lợi ở dataset inside.

| Backend | Dataset | N | Median (ms) | Khoảng campaign (ms) |
|---|---|---:|---:|---:|
'''
for k,v in sorted(summary.items()):
 b,d,n=k
 if n==1000000 or (n==1000 and b in ('jolt_before_4','jolt_after_4','jolt_optimize_4')):
  text+=f'| {b} | {d} | {n} | {statistics.median(v)/1e6:.4f} | {min(v)/1e6:.4f}–{max(v)/1e6:.4f} |\n'
text+='\nCác tỷ lệ dưới dùng median của campaign-median; spatial timing là batch 1.000 query:\n\n'
for d in ('mixed','inside','outside'):
 a=statistics.median(summary['neon',d,1000000]);b=statistics.median(summary['neon_cull',d,1000000]);text+=f'- NEON culling, {d}: {a/b:.2f}× so với NEON cũ.\n'
for d in ('uniform','clustered'):
 a=statistics.median(summary['jolt_before_4',d,1000]);b=statistics.median(summary['jolt_after_4',d,1000]);cost=statistics.median(summary['jolt_optimize_4',d,1000]);text+=f'- Jolt {d}, 1.000 body: query {a/b:.2f}×; rebuild {cost/1e6:.4f} ms; hòa vốn ước lượng {cost/((a-b)/1000):.1f} query nếu workload không đổi.\n' if a>b else f'- Jolt {d}: không có lợi ích query đáng dùng trong fixture này.\n'
text+='''
Giới hạn quan trọng: query clustered vẫn lấy đều trong vùng rộng, nên có nhiều query trống; chưa đại diện query tập trung trong cụm. Before/after spatial không hoán đổi thứ tự vì mutation tree, do đó vẫn có order/cache bias dù đã warmup. Không ngoại suy speedup spatial sang FPS hoặc pipeline spawn đang thay đổi liên tục.

Kiểm tra: 11/11 CTest Release và ASan/UBSan qua; shader/GPU không thay đổi ở vòng này. Mỗi batch spatial và mỗi backend wind đều kiểm tra correctness. Raw data và summary được lưu riêng; tái lập bằng `python3 scripts/refine_benchmarks.py` rồi `python3 scripts/summarize_refinement.py` sau khi build Release.
'''
update_section(text,"\n## Vòng 2:")
print(text[-2400:])
