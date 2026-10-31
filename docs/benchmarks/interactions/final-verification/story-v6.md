# Tối ưu tương tác giữa vật thể — câu chuyện thí nghiệm

**Kết quả cuối (vòng VI):** game dùng 1 background physics worker trên máy có ít nhất 4 hardware threads; median physics giảm 45,2%, median simulation giảm 40,8% trong campaign windowed mới. Physics p99 tăng 7,3%, simulation p99 tăng 4,2%, CPU frame p99 gần như không đổi. Giữ mọi va chạm và solver setting, có serial override. Những quyết định default ở các vòng trước là lịch sử thử nghiệm; xem vòng VI để biết policy hiện tại.

## Vòng I: từ nghi ngờ đến bằng chứng

**Perf chưa tốt.** Sau tối ưu wind/render, physics vẫn là phần CPU lớn nhất của simulation. Chỉ đo PhysicsSystem::Update chưa chứng minh narrowphase/solver là nguyên nhân. Bắt đầu với cùng scene production (600/1000 requested object, seed 42), fixed dt=1/60, tornado đổi chiều mỗi 60 step; bỏ 60 warmup, đo 300 step. Chạy năm process/config, thứ tự shuffle. Không render trong lượt đo này.

**Profile/deep dive.** Thêm executable interaction_bench. ContactListener tùy chọn đếm OnContactAdded/Persisted, tách dynamic–dynamic và static contacts; đây là manifold callback count, không phải unique body-pair count. Timing campaign tắt listener; diagnostic riêng bật listener và Jolt built-in Profiler. Trace HTML ở profile/ và aggregates.csv là các scope inclusive ticks của một frame (step 180), không cộng các hàng nested với nhau, không quy ticks thành ns. Threaded profile chưa nằm trong trace này.

Ở frame chụp, FindCollisions ~55% root Update, convex-vs-convex ~26%, broadphase FindCollidingPairs ~17%, SolveVelocityConstraints ~21%, SolvePositionConstraints ~11%. Các scope con nằm trong scope cha. Build Jobs gần root không phải scheduler chiếm gần 100%: JobSystemSingleThreaded chạy job inline khi dependency sẵn sàng, do đó stack tạo/dispatch job bao trùm phần thực thi. Không tối ưu nhầm wrapper dựa trên inclusive percentage.

**Tìm nguyên nhân bằng can thiệp.** Tắt dynamic–dynamic collision ở object-layer filter, vẫn giữ static–dynamic. Đây chỉ là chẩn đoán: nó làm đổi trajectory và contact workload, không phải optimization hợp lệ để deploy. Không dùng tỷ lệ chênh để tuyên bố chính xác bao nhiêu phần trăm thời gian do collision trên cùng trạng thái; state evolution đã thay đổi.

**Thử phương pháp giữ semantics.** Giữ mọi collision và solver setting, chuyển riêng Jolt job backend sang persistent JobSystemThreadPool với 1/2/4 background worker. Main thread cũng thực thi jobs khi chờ barrier: số worker không phải tổng concurrency. Khác với pool wind không thắng end-to-end trước đây, Jolt có jobs collision/island/solver thực sự song song. Capacity, timestep, shapes, friction, restitution và lực giữ nguyên.

**Benchmark lại.** Median của năm campaign-median physics time:

| Requested N | Background workers | Dynamic collisions | Physics median (ms) | Min–max campaign (ms) | Exact checksum/active trajectory |
|---:|---:|---|---:|---:|---|
| 600 | 0 | 0 | 0.204 | 0.198–0.208 | diagnosis_changes_semantics |
| 600 | 0 | 1 | 0.682 | 0.674–0.689 | True |
| 600 | 1 | 1 | 0.435 | 0.418–0.448 | True |
| 600 | 2 | 1 | 0.345 | 0.334–0.356 | True |
| 600 | 4 | 1 | 0.335 | 0.334–0.351 | True |
| 1000 | 0 | 0 | 0.341 | 0.329–0.343 | diagnosis_changes_semantics |
| 1000 | 0 | 1 | 1.580 | 1.554–1.593 | True |
| 1000 | 1 | 1 | 0.961 | 0.956–0.985 | True |
| 1000 | 2 | 1 | 0.752 | 0.729–0.758 | True |
| 1000 | 4 | 1 | 0.622 | 0.612–0.624 | True |

Diagnostic contact totals (300 steps, listener enabled):

- collision=0: dynamic manifold callbacks 0; static callbacks 21706.
- collision=1: dynamic manifold callbacks 54001; static callbacks 32911.

**Kết luận hiện tại.** Có bằng chứng vật thể tương tác là chi phí đáng kể, và worker pool của Jolt là ứng viên giữ semantics tốt hơn việc song song riêng wind. Physics 600 object từ ~0,682 ms xuống ~0,335 ms với 4 background workers; 1000 object ~1,580 → ~0,622 ms. Hai worker tại 600 object gần bốn worker, có thể là trade-off CPU tốt hơn. Checksum vị trí và active count khớp serial từng step cho các lượt collision-on đã đo, nhưng chưa thay thế kiểm tra toàn state/tolerance hoặc collision correctness.

Chưa bật worker pool làm default game trong vòng này. Cần tiếp: test toàn body state và collision invariants với nhiều worker; đo scene nhỏ để tìm crossover; kiểm tra end-to-end/windowed replay và sanitizer; chọn policy sau số liệu. Không gọi goal hoàn tất chỉ từ speedup của phase physics. Năm process trên một máy chưa chứng minh scaling chung, không có pin affinity/thermal control.

## Tái lập và artifact

```sh
cmake --build --preset perf -j 8
python3 scripts/benchmark_interactions.py --output /tmp/new-interaction-campaign
python3 scripts/summarize_interactions.py
```

Summarizer hiện đọc campaign lịch sử tại interactions/campaign; nếu thu output khác, cần phân tích đúng thư mục đó, không dùng số cũ như kết quả mới. Raw gồm update/wind/physics ns, active count và checksum. Profile chạy riêng từ artifact directory bằng đường dẫn tuyệt đối tới interaction_bench, arguments `N workers collisions telemetry profile`; profile option cần JPH_PROFILE_ENABLED (Release hiện bật). Không chạy profile/telemetry cùng performance comparison rồi xem hai số như không có instrumentation overhead.

## Vòng II: phản chứng ở scene nhỏ và kiểm tra semantics

**Vấn đề mới sau khi thử pool:** speedup 600/1000 object chưa đủ để chọn mặc định cho mọi scene. Có thể chi phí wakeup/barrier lớn hơn công việc collision ở scene nhỏ.

**Deep dive bằng sweep:** đo lại năm process/config trên 25/100/300/600/1000 requested object, giữ mọi collision và solver setting. Listener tắt trong timing. Ở N=25, collision on/off gần như cùng median, cho thấy fixture lúc này ít tương tác dynamic; không thể dùng kết luận dense scene cho sparse scene.

**Thử 0/1/2/4 background worker:** các cấu hình nhỏ thực sự chậm hơn serial. Bảng dưới lấy median của năm campaign median, cùng campaign scaling/:

| Requested N | Serial physics ms | 2 workers ms | 4 workers ms | Quyết định |
|---:|---:|---:|---:|---|
| 25 | 0.012 | 0.017 | 0.032 | Giữ serial cho workload này |
| 100 | 0.041 | 0.049 | 0.070 | Giữ serial cho workload này |
| 300 | 0.213 | 0.134 | 0.155 | Pool có lợi; không coi đây là threshold phổ quát |
| 600 | 0.647 | 0.326 | 0.333 | Pool có lợi; không coi đây là threshold phổ quát |
| 1000 | 1.482 | 0.629 | 0.617 | Pool có lợi; không coi đây là threshold phổ quát |

**Correctness thay vì chỉ checksum:** physics_workers_tests chạy hai seed (42/2026), 100/600 object, 120 step đổi hướng tornado, serial/1/2/4 worker. So chính xác position, center of mass, quaternion, linear/angular velocity từng body, body count và active count. Fixture hai box chồng nhẹ + floor kiểm tra dynamic collision bật/tắt và không rơi xuyên sàn ở 0/2/4 worker. Không giảm solver iterations, đổi shape hay bỏ collision để lấy speedup.

**Kết luận/chọn candidate:** built-in game luôn yêu cầu 600 object nên chọn hai background worker để thử end-to-end; hai worker đạt gần/bằng bốn với ít thread hơn trong sweep này. Simulation/PhysicsWorld API mặc định vẫn serial để caller/benchmark explicit chọn policy. Đây không phải heuristic tự động dựa riêng vào body count cho mọi scene.

## Vòng III: benchmark lại trong game, tích hợp và giới hạn tail

**Câu hỏi:** physics phase nhanh hơn có giảm simulation CPU của game thật không, và có thực sự tăng FPS không?

**Phương pháp:** renderer instanced không đổi, collision bật, seed 42, fixed dt, cùng trajectory. Ba process cho mỗi cấu hình Jolt 0/2/4 worker, 210 frames/run, bỏ 30 đầu. Tất cả cặp raw phải khớp physics checksum/active/resolution. Ghi worker count trong từng row. Log tại windowed/.

| Background workers | Median simulation ms | Median campaign p99 simulation ms | Median CPU frame ms | Median campaign p99 CPU frame ms |
|---:|---:|---:|---:|---:|
| 0 | 2.027 | 3.679 | 8.326 | 9.761 |
| 2 | 0.769 | 5.086 | 3.608 | 11.043 |
| 4 | 0.922 | 4.975 | 8.315 | 11.796 |

**Kết luận không tô hồng:** hai worker giảm median simulation ~2,027 → ~0,769 ms trong campaign windowed này. Nhưng p99 simulation ~3,679 → ~5,086 ms, p99 CPU frame cũng xấu hơn trong mẫu ngắn. GPU time/presentation cadence thay đổi giữa cấu hình; median CPU frame 3,608 ms của hai worker không được diễn giải là FPS tăng 2,3×. 180 measured frames/process và ba process chưa đủ kết luận rare-tail; OS scheduling/thermal/presentation không được khóa. Optimization nhắm median/headroom CPU cho scene mặc định, không hứa cải thiện mọi percentile hoặc mọi máy.

**Triển khai:** Application dùng hai background Jolt worker nếu hardware_concurrency >=4, còn máy ít core hoặc không rõ thì serial. QUACK_PHYSICS_WORKERS=0..16 cho phép override, 0 là fallback serial để so hoặc ưu tiên ít thread. Không tự đổi solver/collision theo load. Wind pool riêng vẫn off. Chọn mặc định này dựa built-in 600-object scene và sweep đã đo; nếu đổi scene/collider density phải benchmark lại. Người dùng có yêu cầu tail chặt nên thử serial và so long-run trên máy đích.

**Validation:** Release và Debug kiểm tra 26/26 tests qua, gồm toàn-state worker trajectories và reject worker count ngoài range. App thật thêm 30-frame replay với Jolt 2 worker, ASan/UBSan halt_on_error + Metal Validation; log kiểm tra riêng, không dùng làm performance data. Vendor Jolt không được instrument toàn bộ, nên không gọi đây là sanitizer proof cho mọi nội bộ Jolt/thread race. Không có full-engine TSan claim.

## Lệnh chạy và đường dẫn evidence mới

```sh
cmake --build --preset perf -j 8
ctest --preset perf
./build/perf/Release/physics_workers_tests
python3 scripts/benchmark_interactions.py --output /tmp/physics-scaling-new
python3 scripts/benchmark_physics_windowed.py --output /tmp/physics-windowed-new
QUACK_PHYSICS_WORKERS=0 QUACK_REPLAY_FRAMES=210 ./build/perf/Release/quacknado
QUACK_PHYSICS_WORKERS=2 QUACK_REPLAY_FRAMES=210 ./build/perf/Release/quacknado
```

Runner không ghi đè campaign cũ. Profile HTML, aggregates và diagnostic contact CSV ở profile/ và campaign/; sweep mở rộng ở scaling/; windowed comparisons ở windowed/; full-state tests/smoke ở verification/. Summarizer vòng I nay có --campaign để chọn dữ liệu đầu vào và giữ các vòng II/III khi chạy lại. Không dùng mặc định lịch sử như thể là kết quả từ /tmp mới. Source snapshot cuối và manifest artifact được lưu riêng, không sửa metadata gốc của các lượt đã chạy.

**Kết luận toàn vòng:** nguyên nhân có bằng chứng là công việc collision/contact/solver lớn trong scene đông; tìm thêm thread cho wind không giúp phần đó. Dùng scheduler của Jolt cho chính jobs tương tác giữ collision semantics, đạt khoảng 1,98× physics ở 600 object và 2,36× ở 1000 object với hai worker trong sweep mới; game đo được giảm median simulation. Đã tích hợp và kiểm tra, đồng thời ghi rõ crossover, fallback và tail regression. Đây là optimization có trade-off, không phải chiến thắng tuyệt đối hoặc lý do bỏ tương tác vật thể.


## Vòng IV: long-run phản chứng — chưa chốt worker default

Mẫu ngắn của vòng III chưa đủ tin cậy cho p99. Chạy thêm ba process/mode, 900 frames/run, bỏ 60 đầu, so serial và 2 worker, giữ instancing/collision/seed/trajectory. Toàn bộ paired physics checksum/active/resolution vẫn khớp; raw 5.400 frames ở windowed-long/.

| Metric | Serial | 2 workers |
|---|---:|---:|
| Median simulation ms | 2,220 | 1,555 |
| Median campaign p99 simulation ms | 4,348 | 5,138 |
| Median CPU frame ms | 8,329 | 8,317 |
| Median campaign p99 CPU frame ms | 9,907 | 11,734 |

Median simulation giảm khoảng 30%, nhưng p99 simulation tăng khoảng 18% trong mẫu này. GPU command median 1,381 vs 1,345 ms gần nhau hơn lượt ngắn; không còn dùng median frame 3,608 ms của sample trước để suy throughput. Vẫn chưa pin OS frequency/thermal/affinity; đây là dấu hiệu tail cần điều tra, không chứng minh nguyên nhân scheduler bằng số liệu tổng.

**Quyết định sửa sau bằng chứng mới:** ứng viên default hai worker của vòng III chưa được chốt; Application tạm giữ serial mặc định. QUACK_PHYSICS_WORKERS=2 vẫn là đường threaded đã kiểm tra, có median tốt hơn khi chấp nhận trade-off. Không giảm fidelity/collision để che tail. Tại thời điểm kết thúc vòng IV, goal còn active: cần profile scheduling/barrier/QoS hoặc thử policy khác để giữ lợi ích tương tác mà kiểm soát tail. Việc giữ baseline không phải kết luận bỏ optimization; đây là lần phản chứng trong chính vòng lặp yêu cầu.


## Vòng V: kiểm tra giả thuyết QoS và tách phase trong frame chậm

**Perf chưa tốt:** lượt dài ở vòng IV cho thấy worker giảm median nhưng tăng p99. Không suy ra scheduler là nguyên nhân chỉ từ thời gian tổng. Cần đối chứng bằng thay đổi scheduling có kiểm soát, đồng thời đo riêng wind/physics trong replay thật.

**Deep dive:** `external/JoltPhysics/Jolt/Core/JobSystemThreadPool.h` cho phép `SetThreadInitFunction` trước `Init`. Worker gọi callback trước khi nhận jobs trong `ThreadMain`. Vì vậy có thể yêu cầu QoS từ chính worker mà không sửa vendor, không đổi dependency graph, collider, contact cache hay solver iterations. Theo [tài liệu QoS của Apple](https://developer.apple.com/library/archive/documentation/Performance/Conceptual/power_efficiency_guidelines_osx/PrioritizeWorkAtTheTaskLevel.html), pthread có thể yêu cầu class qua `pthread_set_qos_class_self_np`. Đây là yêu cầu tới scheduler, không phải pin P-core hoặc cam kết deadline. API contract/return code được đối chiếu với header SDK `pthread/qos.h`.

**Phương pháp thử:** thêm `PhysicsOptions::worker_user_initiated` (mặc định false), `QUACK_PHYSICS_QOS=0/1` (1 cần workers >0). Worker yêu cầu USER_INITIATED, kiểm tra return code và requested class; nếu thất bại thì Step báo lỗi, không âm thầm benchmark một mode chưa có hiệu lực. Đây không đo effective QoS theo từng thời điểm, residency trên P/E core hoặc số lần preempt. Không yêu cầu realtime hoặc busy-spin.

Replay nay bật các phase clocks sẵn có và ghi `physics_ns`, `wind_ns`, `physics_qos` trong CSV. Overhead clocks áp dụng như nhau cho cả ba config mới; không trộn số mới với campaign cũ để quy toàn bộ khác biệt cho QoS. `scripts/benchmark_physics_qos.py` chạy 5 process/config, 900 frames/process, bỏ 60 đầu, shuffle serial / 2 worker không đổi QoS / 2 worker USER_INITIATED. Mỗi campaign kiểm tra checksum, active count và resolution từng frame khớp serial. Đây là replay game thật, chưa khóa thermal/frequency/affinity; p99 là median của 5 process p99, không phải pooled p99 hay cam kết rare-tail.

**Correctness trước timing:** 28/28 Release tests qua. Full-state physics test được mở rộng thêm mode 2 worker USER_INITIATED, so position/COM/quaternion/linear/angular velocity với serial từng step ở hai seed, scene 100/600 object. Thêm reject invalid QoS và reject QoS=1 khi serial. Kết quả timing và quyết định sẽ bổ sung sau campaign; default vẫn serial trong thời gian thử.


**Benchmark lại QoS (ms):** dữ liệu mới `qos-windowed/`, 13.500 raw frames, 12.600 measured frames. Các bảng dưới dùng median của 5 process medians và median của 5 process p99, cùng cấu hình renderer.

| Config | Physics median | Physics p99 | Simulation median | Simulation p99 | CPU frame p99 |
|---|---:|---:|---:|---:|---:|
| Serial | 1,638 | 3,525 | 1,952 | 4,082 | 9,999 |
| 2 worker, QoS không đổi | 0,836 | 3,906 | 1,179 | 4,507 | 10,022 |
| 2 worker, USER_INITIATED | 0,629 | 3,918 | 0,899 | 4,540 | 10,288 |

**Kết luận phản chứng:** QoS giảm median physics thêm khoảng 25% so với 2 worker không đổi QoS, nhưng p99 physics gần như không giảm. Không thể kết luận chính sách QoS này đã chữa tail, và không tự bật nó mặc định. Trong top 10 frame simulation chậm nhất của mỗi process (150 frame), median tỷ lệ physics/simulation là 89,3% (`slow-frames.csv`). Điều này định vị phase có wall time lớn, chưa phân biệt thời gian tính toán với preemption/waiting. Chưa có threaded scheduling trace đủ để quy nguyên nhân cho semaphore, migration P/E core hay priority inversion. Không ghi những giả thuyết đó thành kết luận.

## Vòng VI: giảm mức song song, chọn policy và kiểm tra lại

**Perf chưa đạt trade-off mong muốn:** 2 worker có median tốt nhưng p99 simulation cao hơn serial khoảng 10–11% ở lượt mới; QoS không sửa được. Thử 1 background worker (main thread vẫn cùng chạy jobs) để giảm mức song song và chi phí phối hợp, thay vì bỏ contact hoặc giảm độ chính xác solver.

**Benchmark:** cùng runner, đổi `--configs 0:0,1:0,1:1`, output `one-worker-windowed/`. Tiếp tục 5 process/config, 900 frames/process, 60 warmup; 13.500 raw frames. Dùng serial đối chứng mới ngay trong campaign này, không lấy baseline của vòng V. Checksum/active/resolution khớp từng frame cho tất cả cặp trong cả hai campaign mới.

| Config | Physics median ms | Physics p99 ms | Simulation median ms | Simulation p99 ms | CPU frame p99 ms |
|---|---:|---:|---:|---:|---:|
| Serial | 1,763 | 3,517 | 2,106 | 4,144 | 9,909 |
| 1 worker, QoS không đổi | 0,966 | 3,774 | 1,246 | 4,316 | 9,909 |
| 1 worker, USER_INITIATED | 0,919 | 3,754 | 1,206 | 4,409 | 9,858 |

**Kết luận định lượng:** 1 worker không đổi QoS giảm median physics khoảng 45,2% (1,82×) và median simulation khoảng 40,8%. Physics p99 tăng khoảng 7,3%, simulation p99 khoảng 4,2%; CPU frame p99 gần như bằng serial trong phép đo này. Cả năm paired process đều có median physics và simulation tốt hơn serial. Tuy nhiên paired physics-p99 ratio chạy từ 0,644 đến 1,164; CPU-frame-p99 ratio 0,941–1,043. Đây là biến động thực cần giữ trong báo cáo, không phải chứng minh tail luôn bằng hoặc tốt hơn. Median CPU frame ~8,33 ms ở mọi mode, không có bằng chứng FPS tăng. QoS ở 1 worker thêm ít lợi ích median và simulation p99 cao hơn, nên không chọn làm mặc định.

**Policy production cuối:** built-in scene luôn yêu cầu 600 object; chọn 1 background Jolt worker khi `hardware_concurrency() >= 4`, ngược lại/không rõ thì serial. Đây là lựa chọn ưu tiên giảm thời gian CPU trung vị với mức song song thấp nhất đã đo có lợi cho scene này, chấp nhận regression nhỏ đã công bố của physics/simulation p99. Không tuyên bố đã giải quyết mọi frame chậm hoặc đạt mục tiêu hard realtime. API Simulation/PhysicsWorld vẫn mặc định serial để caller chọn rõ. `QUACK_PHYSICS_WORKERS=0` là fallback cho ưu tiên tail/ít thread; 1..16 cho thử scaling; QoS off mặc định và `QUACK_PHYSICS_QOS=1` là thử nghiệm opt-in. Máy/scene khác cần benchmark lại; scene nhỏ 25/100 object đã chứng minh pool có thể chậm hơn.

Đây là quyết định sau hai lần phản chứng, không phải “p99 đã hết nghẽn”. Nếu yêu cầu sản phẩm là p99 physics tuyệt đối không được tăng thì chọn serial; dữ liệu hiện tại không chứng minh bất kỳ mode threaded nào đáp ứng yêu cầu đó. Mục tiêu vòng này là cải thiện interaction throughput có kiểm chứng, giữ fidelity và công khai trade-off; không đổi mục tiêu thành một claim FPS hoặc latency không có bằng chứng.

**Tái lập hai thử nghiệm mới:**

```sh
cmake --build --preset perf -j 8
python3 scripts/benchmark_physics_qos.py --output /tmp/new-qos-windowed
python3 scripts/benchmark_physics_qos.py --configs 0:0,1:0,1:1 --output /tmp/new-one-worker
python3 scripts/analyze_physics_qos.py /tmp/new-one-worker
cmake --build --preset check -j 8
ctest --preset perf
ctest --preset check
python3 scripts/validate_physics_replay.py --output /tmp/new-physics-smoke
```

Runner từ chối ghi đè campaign gốc; analyzer chỉ tái tạo `per-process.csv`/`slow-frames.csv`, không sửa raw/metadata/summary. `measured-source.tar.gz` và `environment.json` trong từng campaign lưu source đúng lúc đo và hash binary. Source snapshot cuối lưu riêng: thay đổi default và mở rộng tests sau timing không được giả làm source byte-identical với binary đã đo; các lượt đo chọn worker/QoS explicit nên quyết định default sau đó không đổi backend được đối chứng. Không có affinity/frequency/thermal pinning. Không đo energy, không claim tiết kiệm pin hoặc worker residency trên P-core. Profiler là trace serial một frame, không phải threaded scheduler trace. QoS dựa trên API macOS, không phải portable scheduler tuning.


**Kiểm chứng trên source cuối:** Release 28/28 CTest (3,28 s), Debug ASan/UBSan 28/28 (50,93 s). Full-state test gồm serial/1/2/4 worker và 1/2 worker có QoS, hai seed, scene 100/600 object, 120 step; so mọi position/COM/quaternion/linear/angular velocity và active count. Collision filtering/floor invariants vẫn qua. App Debug thật chạy 4 config × 60 frame với ASan/UBSan halt-on-error và Metal Validation: default thực sự ghi 1 worker, serial ghi 0, QoS ghi 1/2; checksum/active/resolution khớp. Không thấy lỗi sanitizer/validation trong logs. Vendor Jolt không được instrument toàn bộ; allocation probe và UBSan vptr vẫn có ngoại lệ đã mô tả trong report gốc. Không claim full-engine TSan.

Log build/CTest và audit ở `final-verification/`; app smoke ở `final-smoke/` và không dùng làm performance evidence. Audit kiểm tra đủ 30 process / 27.000 raw frames mới, thứ tự frame 0..899, paired replay state và source archive khớp metadata của từng campaign. `final-verification/source-hashes.json` / `source-snapshot.tar.gz` là source cuối; `source-delta-vs-measured.json` liệt kê khác biệt với source lúc đo, không ghi đè snapshot cũ. `artifact-sha256.json` định danh toàn bộ artifact interaction hiện tại (trừ chính manifest).

| Yêu cầu của vòng tối ưu tương tác | Bằng chứng trực tiếp |
|---|---|
| Xác định chi phí tương tác | Trace/profile scopes vòng I; diagnostic collision/contact riêng, công bố semantics thay đổi |
| Triển khai cải tiến dùng trong game | Jolt persistent backend, policy 1 worker cho scene mặc định; override/fallback serial |
| Thử và phản chứng | Sweep 0/1/2/4, scene nhỏ, long-run p99, QoS 2 worker và giảm còn 1 worker |
| Benchmark có đối chứng và log | Raw/summary/metadata/process logs; 5 process/config; snapshot source/binary hash cho hai campaign mới |
| Giữ correctness | Full-state tests, collision invariants, paired replay và Debug app smoke |
| Viết câu chuyện đầy đủ | Sáu vòng perf → deep dive → giả thuyết → thử → đo lại → kết luận; kết quả không thắng được giữ nguyên |

Mức cải thiện được chứng minh là thời gian CPU trung vị của physics/simulation trên workload đã đo. Tail regression nhỏ vẫn tồn tại và đã được chấp nhận rõ trong policy, có đường serial cho ưu tiên latency; không có claim FPS, energy, mọi scene hay mọi máy. Profiling scheduler sâu hơn để tiếp tục giảm tail là hướng cải tiến tiếp theo, không phải kết quả đã đạt trong vòng này.
