# Breakthrough: lag khi nhiều vật thể va chạm (1.000 object)

**Kết quả:** đã tái hiện hitch lớn, tìm được đường build không tối ưu và kiểm chứng cải thiện cả tail. Physics của đống 1.000 box luôn active giảm median **44,391 → 1,872 ms**, p99 **47,043 → 2,570 ms**. Trong replay real-time scene game 1.000 object, p99 CPU frame **137,469 → 20,655 ms**; frame >33,33 ms giảm **373/900 → 1/900**. Game mặc định được build Release tại chính đường chạy `build/default/quacknado`; collider, solver, timestep và collision filtering giữ nguyên. Các số là bằng chứng trên máy/fixture đã đo, không phải cam kết mọi frame đều hết khựng.

## Từ phản ánh của người chơi đến nguyên nhân

**Perf chưa tốt.** Người chơi vẫn thấy khựng rõ khi vật thể bắt đầu va chạm, còn lúc đã đứng yên thì ổn. Những lượt tối ưu worker trước đây chủ yếu đo binary Release, replay 600 object và một physics step/frame. Worktree hiện tại lại chạy interactive với 1.000 object; `build/default` có `CMAKE_BUILD_TYPE` rỗng. Đây là thiếu sót về độ bao phủ của benchmark trước, không thể dùng các median cũ để phủ nhận lag đang xảy ra.

**Deep dive vào đúng executable.** `compile_commands.json` cho thấy cả `src/simulation/simulation.cpp`, wrapper physics và Jolt `PhysicsSystem.cpp` của default đều không có `-O2/-O3`; bản perf dùng `-O3 -DNDEBUG`. Giữ nguyên binary cũ trước khi rebuild để đo. Chạy ba process/config, N=1000, một worker, fixed dt, 60 warmup + 300 measured step: median physics của empty-build-type **18,071 ms**, Release **0,863 ms** (~20,94×). Cache, compile commands, binary hashes, raw và logs ở `build-diagnosis/`. Đây là phát hiện ban đầu từ binary có sẵn, chưa phải so sánh cùng source đã rebuild; những lượt tiếp theo dùng cùng source simulation/replay/stress fixture.

**Tìm cơ chế khuếch đại lag.** Simulation chạy 60 Hz, nên một physics step đã mất khoảng 18 ms có thể vượt ngân sách 16,67 ms ngay cả trước wind/render. Vòng interactive cộng elapsed vào accumulator, rồi chạy mọi step còn nợ; elapsed mỗi frame bị clamp 100 ms theo policy cũ. Frame chậm tạo thêm step bù ở frame kế tiếp; mỗi frame có thể phải chạy sáu physics step. Không chỉ một phép kiểm tra collision bị chậm: thời gian tính lớn được vòng catch-up khuếch đại thành hitch >100 ms và một phần wall time bị bỏ bởi clamp.

**Thử nghiệm giữ nguyên hành vi.** Tách policy hiện có vào `FixedStep`, dùng chung trong interactive và replay real-time; không giới hạn bớt số step bù, không đổi 1/60 hoặc clamp để làm đẹp số. Replay thêm `QUACK_SCENE_COUNT`, `QUACK_REPLAY_CLOCK=fixed/realtime` và `QUACK_REPLAY_WIND=0/1`, log số step/frame, total ticks, elapsed, remainder và thời gian từng phase. Mặc định game/replay hiện là 1.000 object; các runner lịch sử nay explicit chọn 600 để giữ fixture cũ. Wind input script được index theo simulation tick. Fixed replay có một tick/frame; real-time replay dùng elapsed thật và chính policy của game.

## Benchmark lại cả workload lẫn frame chậm

`windowed/`: ba process/config, 360 frames/process, bỏ 60 frame đầu; shuffle sáu config gồm hai build × fixed-moving / real-time-moving / real-time-wind-off. Tổng 6.480 raw frames. Hai build dùng cùng source simulation, physics và replay; compiler/build config khác nhau. Tất cả frame có drawable 1280×720 trong campaign này. Replay có phase clocks, checksum và GPU profiling; đây là game thật với input script, không phải số FPS cảm nhận từ một phiên chơi tay. Không có concurrent build trong lúc thu campaign, không pin affinity/frequency/thermal.

| Scene game có wind | Empty build type | Release |
|---|---:|---:|
| Fixed replay: physics median ms | 18,193 | 2,737 |
| Fixed replay: physics p99 ms | 31,069 | 4,978 |
| Real-time: CPU frame median ms | 16,987 | 16,671 |
| Real-time: CPU frame p99 ms | 137,469 | 20,655 |
| Real-time: physics mỗi frame p99 ms (có thể nhiều tick) | 123,389 | 5,040 |
| Real-time: mean steps/frame | 2,797 | 1,000 |
| Real-time: GPU command median ms | 0,525 | 0,534 |

Các median/p99 trên là median của ba process medians/p99; không cộng percentiles của các phase. Bảng cho thấy median frame hầu như không kể được câu chuyện: presentation pacing khoảng 60 Hz, trong khi p99 và số hitch thay đổi rất lớn. GPU command time gần nhau, physics/catch-up CPU là chi phí lớn được quan sát trong lượt này. Không diễn giải physics headless 21× thành FPS game tăng 21×.

Trong 900 measured real-time-moving frames mỗi build: **373 → 1** frame vượt 33,33 ms (~41,4% → 0,11%); **310 → 0** frame có từ sáu step bù; median tổng elapsed bị clamp trong mỗi process **2,083 → 0 giây**. `tail-counts.json` lưu số từng process. Chọn thêm ngưỡng 25/33,33 ms vì ngưỡng 16,67 ms rất nhạy với jitter của vsync 60 Hz. Khi wind tắt và world đã nghỉ, bản cũ cuối lượt có active=0 và physics rất thấp; quá trình settling trước đó vẫn có hitch. Điều này phù hợp với phản ánh “đứng yên ổn, chuyển động/va chạm khựng”, nhưng không tách collision cost thuần khỏi mọi công việc cập nhật bằng một phép trừ median.

**Giới hạn fairness:** hai build không có checksum bitwise giống nhau qua toàn trajectory; kết quả số khác đã được quan sát dưới hai cấu hình compiler/build khác nhau trong hệ contact hỗn loạn; chưa quy khác biệt đó cho một phép toán cụ thể. Raw và `fixed-state-*.json` giữ nguyên mismatch, không ép tolerance lớn để gọi là exact. Real-time hai build cũng đạt số simulation tick khác nhau vì thời gian chạy khác nhau: median total ticks là 1169 và 358 sau 360 frame. Vì vậy đây là bằng chứng trải nghiệm/catch-up theo cùng policy, không phải 900 snapshot vật lý giống hệt nhau. Bổ sung stress fixture để kiểm tra khi cả hai build đều giữ lượng vật thể tương tác lớn.

**Đối chứng dense contact:** `collision_stress_bench` tạo grid 10×10×10 box, hình học/vị trí ban đầu dùng phân số biểu diễn chính xác trong binary; gravity, floor và lực ngang đổi chiều mỗi 30 tick giữ **1.000 body active ở mọi step**. Validate finite position, giới hạn độ cao/tốc độ, không xuyên sàn và quaternion chuẩn hóa ở từng body/step, ngoài khoảng timer. Không đổi shape hay giảm solver iterations. `OptimizeBroadPhase` gọi một lần sau bulk insertion, giống nhau ở cả hai build.

| Dense 1.000 active box, 3 process/build | Empty-build libraries | Release libraries |
|---|---:|---:|
| Physics median ms | 44,391 | 1,872 |
| Median process physics p99 ms | 47,043 | 2,570 |
| Tốc độ tương đối median / p99 | 1× / 1× | 23,72× / 18,30× |

Mỗi process có 360 step, bỏ 60 đầu; telemetry tắt trong timing. Trước khi rebuild default, cùng stress source được compile với đúng flags cũ và link với các archive quack_sim/Jolt chưa thay đổi; argv và hash nằm ở `dense-before-build.json`. Không lấy binary Debug sanitizer làm baseline performance. `dense/` lưu 2.160 raw rows của sáu process và separate contact diagnostic. Diagnostic Release có median **1.567 dynamic / 291,5 static manifold callbacks mỗi step** trong vùng đo; đây không phải số unique pairs. Không có lỗi capacity/update của Jolt trong logs. Stored checksum/active giống nhau đến trước step 25 rồi khác; physical invariants qua cả hai build, không claim toàn body state bitwise giữa hai mức tối ưu compiler. Full-state exactness được kiểm tra riêng giữa các worker mode trong cùng build.

## Triển khai, kiểm chứng và quyết định

`default` preset nay explicit Release, sanitizer off, giữ executable/assets ở build/default để lệnh chạy cũ thực sự dùng binary mới. Direct single-config configure để trống build type cũng chọn Release; explicit Debug của preset check không bị đổi. Cấu hình output của perf/check giữ Release/Debug subdirectory. Startup in build config, requested scene count và worker count để phát hiện chạy nhầm binary. Không thêm fast-math, không đổi collision/solver setting, không che hitch bằng bỏ step. Giữ một background worker và serial override; breakthrough được kiểm chứng ở build/pipeline thay vì nhận một worker count là lời giải cho mọi scene.

`final-verification/build-configuration-audit.json` kiểm tra compile commands của simulation và Jolt đều có -O3 ở default/perf và direct-empty fallback; check vẫn Debug. Unit test fixed-step kiểm tra render 120 Hz vẫn có 60 tick/s, phần dư phân số và policy bù tối đa từ elapsed clamp cũ. Full-state worker test mở rộng scene **100/600/1000**, hai seed, 120 tick, serial/1/2/4 worker và QoS1/QoS2. Release default, perf và Debug hiện **33/33 CTest qua**; dense invariants kiểm tra 1.000 box. Debug app sanitizer/Metal và default-path smoke được ghi riêng sau build cuối, không dùng sanitizer timing làm speedup evidence. Các ngoại lệ instrumentation vendor/allocation/vptr ở report gốc vẫn áp dụng; không claim full-engine TSan.

Lệnh chạy game đã sửa:

```sh
cmake --preset default
cmake --build --preset default -j 8
ctest --preset default
./build/default/quacknado
```

Tái hiện real-time và stress workload:

```sh
QUACK_SCENE_COUNT=1000 QUACK_REPLAY_FRAMES=360 QUACK_REPLAY_CLOCK=realtime ./build/default/quacknado
./build/default/collision_stress_bench 1 0 360
# Telemetry riêng, không trộn vào timing:
./build/default/collision_stress_bench 1 1 360
# Pipeline validation:
cmake --preset check
cmake --build --preset check -j 8
ctest --preset check
python3 scripts/validate_physics_replay.py --output /tmp/new-collision-smoke
```

Các runner `benchmark_collision_breakthrough.py` và `benchmark_dense_contacts.py` nhận `--before/--after`; binary baseline gốc được lưu trước khi nâng cấp default tại build/collision-before. Command lines/hash của baseline thực sự dùng trong lượt dense đã được ghi tại dense-before-build.json. Thư mục build không phải artifact source được commit. Recipe sau là cách dựng baseline từ source với build type explicit `Unoptimized`, giữ các flags không optimization; recipe tái dựng này chưa chạy end-to-end trong vòng đo hiện tại, không được giả làm provenance của số liệu đã thu:

```sh
cmake -S . -B build/collision-unoptimized -G Ninja -DCMAKE_BUILD_TYPE=Unoptimized -DCMAKE_CXX_FLAGS_UNOPTIMIZED=-g -DQUACK_FLAT_OUTPUT=ON
cmake --build build/collision-unoptimized --target quacknado collision_stress_bench copy_shader copy_models -j 8
python3 scripts/benchmark_collision_breakthrough.py --before build/collision-unoptimized/quacknado --after build/default/quacknado --output /tmp/new-collision-windowed
python3 scripts/benchmark_dense_contacts.py --before build/collision-unoptimized/collision_stress_bench --after build/default/collision_stress_bench --output /tmp/new-dense-contacts
```

Lưu cache/compile commands của baseline mới và kiểm tra optimization flags trước khi so; một process mang tên Debug hoặc Unoptimized tự nó chưa chứng minh mọi dependency đã tắt optimization. Không dùng Debug sanitizer làm baseline performance. Runner từ chối ghi đè raw/metadata/summary cũ. Không sửa config của binary đang chạy để dựng baseline, không dùng manifest lịch sử cho binary mới.

**Validation app cuối:** 4 × 60 frame Debug, scene 1000, ASan/UBSan halt-on-error + Metal Validation qua default/serial/QoS1/QoS2; checksum/active/resolution khớp từng frame, không thấy lỗi trong logs (`debug-smoke/`). Full-state exactness giữa worker modes được test riêng; checksum replay không thay thế toàn state. Default Release vừa rebuild chạy thêm 360 frame real-time tại đúng đường build/default/quacknado: startup xác nhận `Build=Release scene=1000 physics_workers=1`, p99 frame **20,656 ms**, không frame nào trong 300 measured frames vượt 33,33 ms hoặc có sáu step bù. Đây là smoke một process xác nhận đường chạy đã sửa, không thay cho ba-process comparison ở trên. Debug 33 tests chạy khoảng 122 s; Release default/perf đều 33/33. Build logs không có compiler warning/error mới; `git diff --check` sạch.

Log/configuration audit/source snapshot cuối tại `final-verification/`; `source-delta-vs-measured.json` và `application-diff-vs-measured.patch` phân biệt source hiện tại với hai campaign đã đo. Core simulation/physics/render và fixed-step helper không đổi sau windowed campaign; delta app runtime chỉ thêm startup build log. `artifact-sha256.json` trong breakthrough định danh các artifact của vòng này. Catalog/source/story của vòng VI được giữ riêng trong interactions/final-verification, còn interactions/artifact-sha256.json được cập nhật cho cây artifact hiện tại. Không thay metadata hoặc raw lịch sử.


**Kết luận:** breakthrough đo được nằm ở việc sửa đường build người chơi thực sự chạy, đồng thời dùng real-time frame/tail telemetry để phát hiện catch-up avalanche. Các số cũ trên Release/600 object/1 tick mỗi frame đã bỏ sót đúng failure mode này. Có improvement lớn khi 1.000 body tiếp xúc dày và giảm hitch rõ trong replay game; vẫn chưa chứng minh mọi phiên chơi/mọi máy hết lag. Với máy đích hoặc scene mới, tiếp tục kiểm tra p99, hitch counts, steps/frame và startup build config trước khi thay scheduler/solver tiếp.
