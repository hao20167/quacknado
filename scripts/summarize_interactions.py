#!/usr/bin/env python3
import argparse
import csv,html,json,pathlib,re,statistics
parser=argparse.ArgumentParser();parser.add_argument('--campaign',default='docs/benchmarks/interactions/campaign');args=parser.parse_args()
root=pathlib.Path('docs/benchmarks/interactions');campaign=pathlib.Path(args.campaign);profile=root/'profile/profile_chart_interaction.html'
s=profile.read_text().split('var aggregated =',1)[1]
names=json.loads(re.search(r'name: (\[[^\n]*\])',s)[1]);cycles=json.loads(re.search(r'cycles_per_frame: (\[[^\n]*\])',s)[1])
pairs=sorted(zip(cycles,map(html.unescape,names)),reverse=True)
with (root/'profile/aggregates.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['inclusive_ticks','scope']);w.writerows(pairs)
rows=list(csv.DictReader((campaign/'summary.csv').read_text().splitlines()))
text='''# Tối ưu tương tác giữa vật thể — câu chuyện thí nghiệm

## Vòng I: từ nghi ngờ đến bằng chứng

**Perf chưa tốt.** Sau tối ưu wind/render, physics vẫn là phần CPU lớn nhất của simulation. Chỉ đo PhysicsSystem::Update chưa chứng minh narrowphase/solver là nguyên nhân. Bắt đầu với cùng scene production (600/1000 requested object, seed 42), fixed dt=1/60, tornado đổi chiều mỗi 60 step; bỏ 60 warmup, đo 300 step. Chạy năm process/config, thứ tự shuffle. Không render trong lượt đo này.

**Profile/deep dive.** Thêm executable interaction_bench. ContactListener tùy chọn đếm OnContactAdded/Persisted, tách dynamic–dynamic và static contacts; đây là manifold callback count, không phải unique body-pair count. Timing campaign tắt listener; diagnostic riêng bật listener và Jolt built-in Profiler. Trace HTML ở profile/ và aggregates.csv là các scope inclusive ticks của một frame (step 180), không cộng các hàng nested với nhau, không quy ticks thành ns. Threaded profile chưa nằm trong trace này.

Ở frame chụp, FindCollisions ~55% root Update, convex-vs-convex ~26%, broadphase FindCollidingPairs ~17%, SolveVelocityConstraints ~21%, SolvePositionConstraints ~11%. Các scope con nằm trong scope cha. Build Jobs gần root không phải scheduler chiếm gần 100%: JobSystemSingleThreaded chạy job inline khi dependency sẵn sàng, do đó stack tạo/dispatch job bao trùm phần thực thi. Không tối ưu nhầm wrapper dựa trên inclusive percentage.

**Tìm nguyên nhân bằng can thiệp.** Tắt dynamic–dynamic collision ở object-layer filter, vẫn giữ static–dynamic. Đây chỉ là chẩn đoán: nó làm đổi trajectory và contact workload, không phải optimization hợp lệ để deploy. Không dùng tỷ lệ chênh để tuyên bố chính xác bao nhiêu phần trăm thời gian do collision trên cùng trạng thái; state evolution đã thay đổi.

**Thử phương pháp giữ semantics.** Giữ mọi collision và solver setting, chuyển riêng Jolt job backend sang persistent JobSystemThreadPool với 1/2/4 background worker. Main thread cũng thực thi jobs khi chờ barrier: số worker không phải tổng concurrency. Khác với pool wind không thắng end-to-end trước đây, Jolt có jobs collision/island/solver thực sự song song. Capacity, timestep, shapes, friction, restitution và lực giữ nguyên.

**Benchmark lại.** Median của năm campaign-median physics time:

| Requested N | Background workers | Dynamic collisions | Physics median (ms) | Min–max campaign (ms) | Exact checksum/active trajectory |
|---:|---:|---|---:|---:|---|
'''
for r in rows:text+=f"| {r['n']} | {r['workers']} | {r['collision']} | {float(r['median_physics_ns'])/1e6:.3f} | {float(r['min_campaign_ns'])/1e6:.3f}–{float(r['max_campaign_ns'])/1e6:.3f} | {r['exact_trajectory_vs_serial']} |\n"
text+='\nDiagnostic contact totals (300 steps, listener enabled):\n\n'
for collision in (0,1):
 contacts=list(csv.DictReader((campaign/f'contacts-collision-{collision}.csv').read_text().splitlines()))
 dynamic=sum(int(r['dynamic_manifolds']) for r in contacts);static=sum(int(r['static_manifolds']) for r in contacts)
 assert collision or dynamic==0
 text+=f'- collision={collision}: dynamic manifold callbacks {dynamic}; static callbacks {static}.\n'
text+='''
**Kết luận hiện tại.** Có bằng chứng vật thể tương tác là chi phí đáng kể, và worker pool của Jolt là ứng viên giữ semantics tốt hơn việc song song riêng wind. Physics 600 object từ ~0,682 ms xuống ~0,335 ms với 4 background workers; 1000 object ~1,580 → ~0,622 ms. Hai worker tại 600 object gần bốn worker, có thể là trade-off CPU tốt hơn. Checksum vị trí và active count khớp serial từng step cho các lượt collision-on đã đo, nhưng chưa thay thế kiểm tra toàn state/tolerance hoặc collision correctness.

Chưa bật worker pool làm default game trong vòng này. Cần tiếp: test toàn body state và collision invariants với nhiều worker; đo scene nhỏ để tìm crossover; kiểm tra end-to-end/windowed replay và sanitizer; chọn policy sau số liệu. Không gọi goal hoàn tất chỉ từ speedup của phase physics. Năm process trên một máy chưa chứng minh scaling chung, không có pin affinity/thermal control.

## Tái lập và artifact

```sh
cmake --build --preset perf -j 8
python3 scripts/benchmark_interactions.py --output /tmp/new-interaction-campaign
python3 scripts/summarize_interactions.py
```

Summarizer hiện đọc campaign lịch sử tại interactions/campaign; nếu thu output khác, cần phân tích đúng thư mục đó, không dùng số cũ như kết quả mới. Raw gồm update/wind/physics ns, active count và checksum. Profile chạy riêng từ artifact directory bằng đường dẫn tuyệt đối tới interaction_bench, arguments `N workers collisions telemetry profile`; profile option cần JPH_PROFILE_ENABLED (Release hiện bật). Không chạy profile/telemetry cùng performance comparison rồi xem hai số như không có instrumentation overhead.
'''
story=root/'story.md'
if story.exists() and '\n## Vòng II:' in story.read_text():text+='\n## Vòng II:'+story.read_text().split('\n## Vòng II:',1)[1]
story.write_text(text);print(text[-1700:])
