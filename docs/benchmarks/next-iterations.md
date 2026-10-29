# Tiêu chí tiếp tục nâng chất lượng

Goal đang active: tiếp tục cải tiến, benchmark, tự kiểm chứng và log cho đến khi portfolio có chiều sâu kỹ thuật và bằng chứng end-to-end vững.

Bằng chứng đã có: CPU/NEON/pool/GPU kernels, spatial trước/sau rebuild, offscreen instancing, Python oracle, raw campaigns, sanitizer và pose-only sync tích hợp game.

Các khoảng trống cần xử lý tiếp, không coi goal hoàn tất khi chúng còn thiếu:
- Measurement integrity: batch calibration cho kernel nhỏ, nhiều process/seed, metadata thống nhất và thống kê uncertainty; tránh percentile của 20 batch bị hiểu thành p99 per-sample.
- Wind pipeline: snapshot/batch compute/apply vào simulation thật với lựa chọn backend, đo overhead và quyết định theo workload; numerical validation toàn trạng thái.
- Renderer: GLB instancing theo mesh/material trong game, GPU resource lifetime đúng khi nhiều frame in flight; đo frame time và kiểm tra ảnh.
- Physics: body/contact telemetry, scenario steady-state có tornado giữ trong scene, scaling Jolt workers độc lập; không đổi physics semantics để đạt speedup.
- Memory: đo allocation steady-state và footprint, xác định lifetime/ownership thay vì claim zero-allocation từ inspection.
- Maintainability: tách fixture/CLI, test từng backend và lỗi đầu vào, cleanup reference API trong production header, format source dễ review.
- Report: cập nhật số liệu được tái lập, ghi cả negative results; mọi claim gắn workload, compiler, hardware và scope.

Không cần ép networking/LLM vào project nếu không giúp câu chuyện chính. Ưu tiên code đã tích hợp và đo đúng hơn thêm nhiều fixture.

Vòng 4 đã đưa snapshot/compute/apply vào Simulation với backend 1/2/4 worker và exact full-state trajectory test; end-to-end chưa thắng direct nên giữ baseline làm mặc định. Ưu tiên kế tiếp: giảm Jolt read/apply overhead và renderer GLB instancing, đo allocation/lifetime; không thêm concurrency chỉ để có nhiều thread.

Vòng 5 tích hợp single-lock BodyState snapshot và bỏ motion-type check trùng với Jolt AddForce; exact trajectory test qua. Tổng update tăng khoảng 1,04× ở 600–1000 requested objects, wind khoảng 1,46×. Có source archive khớp manifest để giữ provenance sau các lần sửa tiếp theo. Renderer GLB batching, allocation instrumentation và calibration benchmark vẫn còn mở.

Vòng 6 đã tích hợp GLB grouping/instancing vào game bằng batch 80 transform, kiểm tra pixel trên chicken/creeper/chest và Metal Validation. Còn cần mixed-model image regression, nhiều frame-in-flight stress, đo frame time cửa sổ thực; allocation và benchmark calibration vẫn mở. Không dùng số CPU submission để tuyên bố FPS tăng cùng tỷ lệ.

Vòng 7 đã bổ sung allocator instrumentation riêng, self-test, regression gate và stack trace Jolt. Chỉ chứng minh không cấp phát qua các đường C++ new được intercept trong trajectory; Jolt broadphase vẫn allocation mỗi step. Chưa đo peak live memory/RSS, chưa profiler allocation trực tiếp từ libc/Metal. Không làm thay đổi numerical path chỉ để đạt zero-allocation.

Vòng 8 đã kiểm tra mixed-model reorder, 79/80/81 instance mỗi model, hidden/invalid và 3 command submissions trước wait; pixel regression qua. GPU instrumented test phát hiện UBSan null-member-call trong metal-cpp ownership wrapper; cần xử lý ownership adapter/binding rồi chạy lại với halt_on_error để không dựa vào exit code recoverable UBSan. Không đánh dấu sanitizer GPU sạch khi log còn finding.

Vòng 9 đã thay ownership handles trong project bằng null-safe intrusive adapter, unit-test retain/release và chạy GPU fail-fast ASan/UBSan + Metal Validation qua 12 case/36 frame. Không sửa vendor hoặc suppress warning. Finding vòng 8 đã có fix và evidence, vẫn giữ log cũ để truy nguyên. GLB submission đo lại giữ lợi ích batching. Các phần còn mở: calibration/uncertainty của benchmark nhỏ, frame-time cửa sổ thật, profiler/RSS, test regression tự động hóa và cleanup cấu trúc source/report để portfolio dễ review.

Vòng 10 có calibrated batch >=5ms ở lượt chọn iterations, 180 process runs, CV và bootstrap interval mô tả; giữ dữ liệu cũ. Pool nhỏ còn nhiễu lớn và không thắng; kernel lớn giữ lợi ích NEON+pool. Việc cần tiếp tục: windowed game frame telemetry/replay, perf regression gating có noise budget, và báo cáo kết luận dễ review từ các experiment đã có. Không thêm backend chỉ để tăng số mục.

Vòng 11 đã đo cửa sổ game thật với deterministic replay, raw CPU/GPU time, paired checksums và resolution gate. Campaign resize không hợp lệ được giữ và loại khỏi thống kê; fixed replay window chạy lại qua. Actual Application Debug smoke ASan/UBSan halt-on-error + Metal Validation qua 30 frame. Submission/GPU giảm nhưng presentation-paced frame median gần như giữ 8,33ms; không claim FPS tăng. Tiếp theo cần perf regression gate có noise policy, chuẩn hóa provenance/campaign output và tài liệu tổng kết dễ review, tránh chỉ tiếp tục append nhiều số liệu mà không kết luận thiết kế.

Vòng 12 có gate PASS/REGRESSION/INCONCLUSIVE, environment/policy/calibration validation, fault injection và 9 unit cases. Same-binary control còn INCONCLUSIVE nên chưa gọi hệ thống fully stable. Report đã có reviewer summary phân biệt production/experiment/negative results. Ưu tiên tiếp: refactor CMake dùng core simulation target chung và cấu trúc benchmark/runner, giảm duplication; cải thiện độ ổn định các workload noisy chỉ khi đo có bằng chứng, không sửa budget để ép pass.

Vòng 13 đã gom simulation/render static targets và sanitizer plain variant, production Release compile entries 31->12; canonical evidence inventory bao gồm cmake/root build files, preset perf/check. 21/21 CTest và GPU fail-fast regression qua. Cần tiếp tục audit tính tái lập/đọc hiểu của toàn bộ bộ benchmark và code (runner error handling, formatting, README dẫn đường), không biến code thành thư viện phức tạp chỉ để có thêm abstraction. Những performance claim mới phải có experiment mới; cleanup build không được tính như speedup kernel.

## Kết thúc scope portfolio sau vòng 14

Audit cuối đã kiểm tra lại yêu cầu ban đầu và evidence trên current tree. Core deliverables hoàn tất: production improvements, low-level/concurrency/GPU experiments, Python reference/analysis, memory/correctness gates, windowed replay, metadata/logs/report và README. Clean build + Release/Debug 24/24, current GPU fail-fast và TSan standalone qua. Goal được đánh giá đạt; các mục mở trước đây còn không bắt buộc (native bindings/LLM/networking/full RSS/more platforms/stronger statistics) là follow-up tùy chọn, không được hiểu là chức năng đã có. Measurement uncertainty của một số workload vẫn được công bố, không ép gate thành PASS.
