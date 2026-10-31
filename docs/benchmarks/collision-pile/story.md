# Đống va chạm 10k khi tắt wind: chọn CPU worker theo quy mô scene

## Triệu chứng và giả thuyết

Khi bật wind, vật thể được tách ra; tắt wind thì chúng rơi thành một đống dày và game lag. Đây là workload khác lượt tối ưu 600–1.000 object trước: source hiện tại đã dùng Release, app mặc định đã được đổi thành 10.000 object, nhưng policy CPU vẫn chỉ có 1 background Jolt worker.

Chụp binary/source trước khi sửa, giữ seed 42, collider, floor, gravity, 60 Hz và cả 10.003 body. Đo không wind qua 1.200 step (20 giây mô phỏng), tách physics/sync/spawn. Baseline physics median sau 60 warmup là khoảng 21 ms; sync khoảng 0,2 ms. Sau 20 giây vẫn gần 10k body active. Jolt chỉ cho cả contact island ngủ khi mọi body đạt tiêu chí ngủ: một đống lớn còn chuyển động không thể được coi là đã ổn định chỉ vì wind đã tắt.

## Đi từ bằng chứng tới thay đổi

1. **Tăng khả năng chia tải trước.** Quét 1/2/4/6/8 background workers trên M1 Pro (8 performance + 2 efficiency cores), main thread cũng tham gia Jolt jobs. Lượt thăm dò 1.200 step: median physics tương ứng khoảng 21,6/17,2/13,0/11,4/11,3 ms; 8 worker không thắng rõ về tail. Tất cả worker config khớp checksum vị trí, active count, population và respawn ở từng step. Đây là checksum diagnostic, không tự thay thế full-state test.
2. **Profile đống tiếp xúc.** Trace riêng có contact telemetry + Jolt profiler tại step 180, 6 worker. Inclusive CPU ticks của SolveVelocityConstraints ≈ 1.050k, FindCollisions ≈ 612k; narrowphase convex trực tiếp ≈ 46k, nhiều manifold đã được cache. Các scope lồng nhau và chạy nhiều thread nên không cộng thành wall time hay tỷ lệ phần trăm frame. `profile-w6/` giữ HTML và bảng aggregate.
3. **Thử giảm solver rồi loại bỏ.** Thử 6/4/8 velocity iterations: median khoảng 9,94/9,88/10,50 ms, so với 11,4 ms ở 10 iterations. Nhưng respawn trong 1.200 step đổi từ 653 thành 841/1.223/671; physics không còn cùng trajectory. Vì worker-only đã chữa được catch-up trong game, giữ nguyên 10 velocity + 2 position iterations, sleep settings và collider. Source/binary thử solver được lưu riêng, không còn option này trong production.
4. **Không áp 6 worker cho mọi scene.** Quét thêm N=1.000/3.000/6.000 với 1/2/4/6 worker. Scene nhỏ có overhead/tail trade-off; N=3.000 còn có thể ngủ phần lớn sau khi ổn định. Policy app giữ 1 worker dưới 4.096, dùng 2 từ 4.096, 6 từ 8.192; giới hạn theo hardware threads trừ 2. Máy báo dưới 4 thread dùng serial; override 0..16 vẫn giữ. Đây là ngưỡng thực dụng theo workload đo trên máy này, không phải công thức tối ưu cho mọi CPU.

Thay đổi production nằm ở `worker_policy.hpp` và thứ tự khởi tạo app: phải đọc scene count trước khi chọn worker mặc định. Không tự đổi worker theo trạng thái bật/tắt wind, không rebuild pool giữa frame. API PhysicsWorld/Simulation vẫn serial mặc định nếu caller không truyền options.

## Đo đối chứng

Campaign `paired/` chạy tuần tự, shuffle before/after, không chạy build/test song song. Ba process/cấu hình, CPU 1.200 step và game 600 frame real-clock, không wind. Timing không bật contact listener, profiler hoặc Metal Validation. Cùng assets/metallib; ghi source snapshot, binary/asset hash và command/env cho từng lượt.

Trung vị của 3 median/p99 theo process; percentile dùng nearest lower index trên các mẫu sau warmup, không gộp tất cả process thành một lượt duy nhất:

| Chỉ số | Trước: 1 worker | Sau: policy tự chọn 6 |
|---|---:|---:|
| CPU physics median / step | 20,523 ms | **11,232 ms** |
| CPU physics p99 / step | 26,610 ms | **18,737 ms** |
| Game CPU frame median | 119,324 ms | **8,654 ms** |
| Game CPU frame p99 | 185,620 ms | **14,543 ms** |
| Frame >33,33 ms, tổng 1.800 frame | 1.413 | **20** |
| Frame phải bù 6 physics step | 1.386 | **1** |

Physics median giảm khoảng 45%; frame p99 giảm khoảng 92%. CPU step vượt ngân sách 16,67 ms gây vòng lặp catch-up: frame chậm → elapsed lớn → nhiều physics step/frame → frame còn chậm hơn. Lượt baseline thường phải làm 6 step/frame; thêm worker đưa phần lớn step xuống dưới ngân sách nên phá được vòng lặp đó. Không bỏ step nợ, không tăng dt, không giảm population để tạo số đẹp.

**Giới hạn tail được giữ nguyên:** p99 các process sau sửa là 59,36 / 14,54 / 14,50 ms; process đầu vẫn có một đợt hitch tới khoảng 230 ms rồi bù step. Chưa đo CPU scheduling để quy chắc hitch đó cho OS hay thermal. Không tuyên bố hết mọi stutter hoặc lấy CPU-frame timer làm FPS/presentation rate. Trước/sau có cùng số frame nhưng khác số physics step và thời gian mô phỏng trong replay real-clock; tính đúng trajectory được so bằng workload CPU có cùng 1.200 step, không so checksum giữa hai replay đã tiến khác thời gian.

**Phản chứng QoS:** 3 cặp 600 frame cùng binary, 6 worker, QoS off/on: trung vị p99 15,05 / 13,95 ms; frame >33 ms 3 / 0. Nhưng chạy tiếp 2.000 frame thì off đạt p99 **16,48 ms**, **0/2.000 frame >33 ms**, **0 six-step frame**, tổng 1.018 physics step; on đạt p99 22,22 ms, 8 frame >33 ms và 2 six-step frame. Không có thắng lợi ổn định để bật QoS mặc định. `qos-probe/` và `long-run/` giữ cả các số không thắng.

Trong mọi CPU step/game frame của campaign 10k đối chứng, population là 10.003, pending 0; mỗi frame game submit 10.002 model objects / 22.258 primitive-instance invocations, floor vẽ riêng. Renderer chờ và kiểm tra command-buffer completion trước khi xuất CSV. Counter này không chứng minh mọi object đều có pixel nhìn thấy qua camera/occlusion.

Kiểm soát khi **bật wind**, 3 cặp CPU × 600 step: update median 15.845 → 9.082 ms, p99 62.507 → 26.398 ms; checksum/active/respawn khớp từng step, đủ 10.003 body. `wind-on-control/` giữ raw/log, không suy lượt CPU này thành FPS.

## Kiểm chứng và tái lập

- `pile_workers_correctness`: mỗi config 1/6 worker chạy 360 step ở 10k, gió tắt 240 step → bật 60 → tắt 60; 12 mốc so chính xác position, center of mass, quaternion, linear/angular velocity của 10.003 body. Mọi step kiểm tra finite pose và đủ body. Fixture riêng kiểm tra body tự ngủ, nhận force thức dậy và không xuyên nền.
- `physics_workers_correctness` mở rộng worker 6 trong các scene 100/600/1.000, hai seed; policy kiểm tra fallback CPU nhỏ/unknown, scene nhỏ và trần worker. Các test collision filtering/floor, respawn phân bố, 10k hai seed, full capacity và fixed-step đều qua.
- Release default, Release perf, Debug check đều **33/37 CTest qua**, gồm test mới. Debug pile full-state mất khoảng 260 giây; giữ timeout 600 giây cho workload này. **4 lỗi có từ trước** ở `wind_neon`, `wind_neon_cull`, `wind_pool_neon4`, `calibrated_kernel` vẫn là numerical mismatch; không sửa wind SIMD trong scope này và không báo suite xanh. Log/full CTest output của cả ba cấu hình nằm cạnh bài và trong `verification/`.
- Debug game 120 fixed-tick frame ở default 10k/policy tự chọn 6, ASan/UBSan halt-on-error + Metal Validation: đủ population/model submissions mỗi frame, GPU commands hoàn tất, không sanitizer/runtime/Metal error. Đây là correctness smoke, không dùng timing Debug để công bố tốc độ. Vendor Jolt/SDL vẫn không được nhận là đã instrument sanitizer toàn bộ.
- Audit xác nhận PhysicsWorld, simulation và spawn sources byte-identical với baseline trước task; final game binary trùng hash binary đo. Chỉ runtime app worker policy đổi. Source snapshot/hash cuối và patch metadata-only của script ở `verification/`. Benchmark runner từ chối ghi đè campaign đã có.

```sh
cmake --build --preset default -j 8
# App hiện mặc định 10k; policy tự chọn theo CPU và scene.
./build/default/quacknado

# No-wind full transient trace: N workers collisions telemetry profiler QoS wind steps
./build/default/interaction_bench 10000 6 1 0 0 0 0 1200
# Override serial/số worker vẫn hoạt động.
QUACK_PHYSICS_WORKERS=1 ./build/default/quacknado
# Lượt dài không wind, có GPU command completion checks.
QUACK_REPLAY_FRAMES=2000 QUACK_REPLAY_CLOCK=realtime QUACK_REPLAY_WIND=0 ./build/default/quacknado
# Chọn output mới để không ghi đè campaign đã đo; before bundle cần asset/metallib cùng phiên bản.
python3 scripts/benchmark_collision_pile.py --output /tmp/new-collision-pile
```

`paired/` chứa 7.200 CPU rows và 3.600 game frames, command/env từng process, metadata, source snapshot và verification. `baseline-source.tar.gz`/`baseline-quacknado` chụp trước thay đổi, `baseline-interaction-bench` giữ instrumentation của baseline; `solver-experiment-*` giữ nhánh thử đã loại bỏ. Baseline app bundle trong `build/collision-pile-before/` dùng binary gốc và assets/metallib giống app sau sửa. Để chạy lại trên bản build khác, truyền `--before-game`, `--after-game`, `--before-cpu`, `--after-cpu`; không dùng lịch sử như số đo của source hiện tại. Script kiểm tra đủ body, model submissions, exact checksum/active/respawn CPU và source/binary/asset không đổi suốt campaign.

Thay đổi script sau campaign chỉ bổ sung đọc CPU/platform thực tế vào metadata thay vì nhãn máy cố định; runtime production không đổi. Snapshot đo và snapshot cuối được lưu riêng. Bài học phỏng vấn: tuning concurrency có điểm đổi theo mật độ/quy mô scene; đo cả physics step lẫn vòng frame catch-up, giữ thử nghiệm giảm solver và QoS không thắng để giải thích vì sao chọn tăng worker mà giữ chất lượng physics.
