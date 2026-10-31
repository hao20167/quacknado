# Sửa spawn đủ 10.000 vật thể

Vấn đề trong lượt kiểm tra được trích ở câu hỏi: yêu cầu 10k nhưng mới có 1.736/2.212/2.912 body tại step 61/180/360. Capacity của Jolt không tạo chỗ trống trong vùng spawn; random retries bị bão hòa rồi ngân sách tối đa 4 placement/step làm hàng chờ tăng lâu. Renderer đúng khi bỏ object chưa có body, nên sửa nằm ở placement và lifecycle.

**Đã sửa:** scene trên 1.000 object dùng shelf packing theo hàng/lớp, x/z vẫn nằm trong nền hiện tại 60×60, khoảng cách box 0,4 m. Sắp theo chiều cao/depth để giảm khoảng trống; đặt lớp đầu trên bounds world của props. Seed 42, N=10k có đỉnh layout ban đầu 16,25 m. Không cần chờ simulation dần tìm chỗ, không đặt các box chồng lên nhau. Scene ≤1.000 giữ random layout cũ để tránh đổi các fixture nhỏ.

Respawn của scene lớn được xử lý hết ngay trong cùng update: trong lượt sync đã có, tính đỉnh AABB world của các body (có xét orientation), rồi cấp các ô grid trên đó. Grid dùng kích thước lớn nhất của các object đang chờ, gap 0,4 m; bước nhảy nguyên tố cùng nhau với số ô giúp đi khắp floor trước khi lặp. Sequence được giữ qua các update, không quay lại góc đầu mỗi frame; burst vượt số ô sẽ lên lớp tiếp theo. Các slot mới cách nhau và nằm trên các body hiện hữu. Đường này không đi qua ngân sách 4 object/step hay 32 random attempts. Small-scene queue vẫn giữ budget cũ. Mỗi update của scene lớn có thêm O(N) scan và R lần placement, không quét mọi cặp; initial packing sort O(N log N), cấp slot O(N), ngoài chi phí engine tạo body/broadphase.

Body capacity 16.384 được giữ; dành chỗ cho floor/chair/dirt house nên giới hạn requested count là **16.381**. Temp allocator tăng 10 → 64 MiB, body-pair/contact limits 4.096 → 65.536 để hỗ trợ contact workload 10k; lỗi update/capacity giờ được báo thành lỗi thực sự thay vì tiếp tục và log lặng lẽ. App bắt exception để thoát có thông báo. Default app vẫn 1.000, public Simulation default 10.000 theo cấu hình hiện tại; dùng biến môi trường để yêu cầu 10k trong game.

## Kết quả kiểm chứng

Các số đo GPU và campaign `verified/` bên dưới thuộc bản sửa đủ population ban đầu. Bản sửa phân bố respawn ngày 2026-10-29 có log riêng trong `respawn-distribution/`; không ghi đè các bằng chứng lịch sử.

Headless fixture seed 42, workers=1, wind bật, dt=1/60: kiểm tra ngay lúc khởi tạo, rồi **mọi step đến 360**. Test đọc actual body states từ Jolt, kiểm tra ID duy nhất, vị trí finite, và sweep AABB ban đầu với clearance để tìm overlap. Không dùng active count làm số đã spawn: body đang ngủ vẫn tồn tại và render được.

| Physics step | Object đã đặt | Chờ spawn | Actual body states |
|---:|---:|---:|---:|
| 0 — ngay sau constructor | 10.003 | 0 | 10.003 |
| 61 | 10.003 | 0 | 10.003 |
| 180 | 10.003 | 0 | 10.003 |
| 360 | 10.003 | 0 | 10.003 |

Số này gồm **10.000 requested objects + 3 props**. `population.csv` có đủ 361 mốc, không chỉ bốn dòng chọn ra. Ở seed 42, init + kiểm tra placement/state của fixture mất khoảng 15,9 ms trong lượt diagnostic này; không coi đó là campaign benchmark startup hoặc FPS.

Game thật chạy 360 fixed-tick frames với Metal Validation, cùng count/seed/worker/input. `verified/raw.csv` kiểm tra mỗi frame:

| Counter | Mỗi frame |
|---|---:|
| Placed, có body hợp lệ | 10.003 |
| Pending | 0 |
| Model objects đưa vào draw | 10.002 |
| Tổng instanceCount qua các model primitive draws | 22.258 |

Floor không thuộc model counter và được vẽ riêng. Chair/dirt house thuộc model counter. Model có nhiều primitive nên một object có thể xuất hiện trong nhiều draw: assets hiện tại chicken=8, boat=3, sheep=2 primitive. Verifier tính số kỳ vọng từ per-model census và GLB loader metadata, không giả định 1 primitive/model. `verified/model-census.json` lưu cách tính. Lượt GPU có tổng **1.341 placement phục hồi/respawn**, nhưng không frame nào hụt population.

Counter được tăng tại đúng chỗ encode lệnh drawIndexedPrimitives, rồi renderer tổng hợp. FinishProfile chờ và kiểm tra tất cả command buffers trước khi in CSV: lượt này GPU hoàn tất không lỗi. Đây là số object/instance **đã submit qua command hợp lệ**, không phải phép đếm visible pixels bằng GPU counter; camera clipping/occlusion vẫn có thể che một số vật thể. Không nhận rằng 10k đều thấy riêng rẽ trên màn hình hoặc mọi máy đạt 60 FPS. Các timing trong raw là diagnostic, có Metal Validation và có thể có build/test chạy đồng thời, không dùng để công bố speedup.

Lượt verifier đầu đã fail vì hardcode 10.004 instance invocations từ giả định model một primitive. Raw lúc đó thực tế vẫn có đủ 10.003 placed / 10.002 model objects; đây không phải thiếu spawn. Đã giữ raw/metadata/source snapshot và giải thích tại `first-verifier-outcome.json`, sửa verifier để lấy census/primitive metadata rồi chạy lại vào `verified/` mới, không ghi đè dữ liệu cũ.

**Tests:** default/perf/check có 36 test, gồm large-scene 10k hai seed, initial placement full capacity 16.381 + props và các physics/ownership/concurrency tests có trước. API overcapacity 16.382 được kiểm tra báo lỗi, app range check đồng bộ. Mixed GLB regression Debug ASan/UBSan + Metal Validation kiểm tra 12 case / 36 frame pixel comparisons, hidden/invalid bodies, ranh giới batch và counter objects/instances với metadata nhiều primitive. Vendor/allocator probe/vptr vẫn có ngoại lệ instrumentation đã mô tả ở report gốc, không có full-engine TSan claim.

## Chạy và tái lập

```sh
cmake --preset default
cmake --build --preset default -j 8
QUACK_SCENE_COUNT=10000 ./build/default/quacknado

# Actual physics population: step 0..360, overlap check và actual body states.
./build/default/large_spawn_tests 10000 360 42

# Game/Metal counts; dùng output mới, runner từ chối ghi đè campaign.
python3 scripts/check_10k_scene.py --output /tmp/new-10k-check
ctest --preset default
```

Source đo của game nằm trong `verified/measured-source.tar.gz`, kèm metadata/binary + asset hashes; final source/log ở `verification/`. Shader/GLB của directory chạy được kiểm tra khớp source assets trước khi chạy. Source cuối chỉ thêm hoàn thiện counter của small-scene queue sau lượt GPU; đường bulk 10k và renderer submission không đổi. Không dùng snapshot lịch sử như source byte-identical của binary mới. `artifact-sha256.json` định danh artifact hiện tại.

## Sửa respawn dồn góc — 2026-10-29

Nguyên nhân: tạo `SpawnShelf` mới mỗi update khiến các đợt respawn ít object luôn lấy ô đầu tại x/z ≈ −29,5. Giữ shelf cho khởi tạo; respawn chuyển sang `RespawnGrid` với sequence xuyên frame, ô được hoán vị khắp nền và tự thêm lớp nếu burst vượt capacity. Đây là phân bố có quy luật, không phải random sampling. Chiều cao vẫn trên ceiling hiện tại, vì vậy khi nhiều vật thể đang bay cao thì respawn cũng cao theo.

Kiểm chứng mới: 75 placement qua 3 lớp không overlap; 25 đợt một object thăm đủ 25 ô và cả 4 góc phần tư. Integration scene 1.001 object: burst 32 object đặt đủ cùng step, sau đó 32 đợt một object không lặp x/z và phủ cả 4 góc phần tư. Release và ASan/UBSan đều qua 4 test spawn/population, gồm 10k hai seed và capacity 16.381.

Lượt headless 10k, seed 42, wind bật, 360 step: **10.003 actual body states, 0 pending ở mọi step**, tổng **1.063 respawn**. Log và source hash ở `respawn-distribution/`. Đây là kiểm chứng correctness, không phải benchmark FPS; lượt này không đo lại GPU. Binary game default đã build lại.

Bộ kiểm tra rộng: 32/36 test qua; 4 test benchmark gió SIMD (`wind_neon`, `wind_neon_cull`, `wind_pool_neon4`, `calibrated_kernel`) báo numerical mismatch. Phần sửa này không thay đổi wind kernel hay benchmark; lỗi được giữ trong log, không tuyên bố toàn bộ suite xanh.
