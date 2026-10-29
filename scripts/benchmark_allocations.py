#!/usr/bin/env python3
from campaign_output import prepare_campaign
from evidence import source_hashes
from report_sections import update_section
import collections,csv,hashlib,io,json,pathlib,subprocess
root=prepare_campaign('docs/benchmarks/allocations')
(root/'metadata.json').write_text(json.dumps({'measured_steps':600,'warmup':60,'seed':42,'sha256':source_hashes()},indent=2))
subprocess.run(['build/perf/Release/quack_alloc','--self-test'],check=True)
text='''\n## Vòng 7: allocation instrumentation, không suy đoán zero-allocation\n
Thêm executable quack_alloc riêng, không liên kết counter vào timing benchmark hoặc game. Global throwing C++ new/new[] và aligned new được đếm bằng atomic; delete tương ứng dùng free. Hook Jolt Allocate/AlignedAllocate/Reallocate riêng, restore hook trước khi hủy Simulation. Vùng đếm chỉ gồm Update sau 60 warmup; output/telemetry/constructor/hủy scene nằm ngoài. Counters bao phủ worker threads. Self-test kiểm tra số call và requested byte chính xác qua scalar/array/aligned C++ cùng ba đường Jolt. CTest bổ sung gate không có C++ allocation trên trajectory direct/pool 100 object.\n
| Requested objects | Workers | C++ new calls | Jolt alloc/realloc calls | Jolt requested bytes | Max Jolt calls/step |\n|---|---:|---:|---:|---:|---:|\n'''
with (root/'raw.csv').open('w') as f:
 writer=None
 for n in (100,600,1000):
  for workers in (0,1,4):
   rows=list(csv.DictReader(io.StringIO(subprocess.check_output(['build/perf/Release/quack_alloc',str(n),str(workers)],text=True))))
   if writer is None:writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader()
   writer.writerows(rows);f.flush()
   cpp=sum(int(r['cpp_calls']) for r in rows);jolt=sum(int(r['jolt_calls']) for r in rows);requested=sum(int(r['jolt_requested_bytes']) for r in rows);peak=max(int(r['jolt_calls']) for r in rows)
   text+=f'| {n} | {workers} | {cpp} | {jolt} | {requested} | {peak} |\n'
text+='''
Stack trace chẩn đoán riêng cho thấy hai call lặp trong Jolt QuadTree::UpdatePrepare và QuadTree::BuildTree, không phải std::vector hoặc tạo worker mỗi frame. Lưu ở allocations/jolt-trace.log. Không sửa vendored Jolt hay tắt broadphase update để đạt con số đẹp. Chưa tuyên bố zero-allocation toàn simulation; điều chứng minh được là không có allocation qua các đường C++ new được intercept trong trajectory đã đo, trong khi Jolt vẫn cấp phát.\n
Requested bytes là tổng kích thước yêu cầu cấp phát, gồm kích thước mới của realloc; không phải live memory, peak RSS hay memory bandwidth. Không intercept malloc/calloc trực tiếp của SDL/libc/Metal hoặc allocation hệ điều hành, không có stack sampling cho mọi allocation. Hai miền counter có thể chồng lấp nếu allocator implementation tương lai gọi C++ new; không cộng chúng thành số allocation toàn process. Jolt hook dùng global allocator, chỉ cài tại điểm không có physics job đang chạy; fixture dùng Jolt single-thread. Diagnostic backtrace chạy riêng và không dùng số đếm của lượt diagnostic để kết luận. `QUACK_ALLOC_TRACE=1` bật hai stack trace đầu.\n
Tái lập: build target quack_alloc, chạy `python3 scripts/benchmark_allocations.py`. Dữ liệu này không được dùng so latency vì atomic/counter overhead thay đổi timing. Giữ game/benchmark timing không có instrumentation.\n'''
update_section(text,'\n## Vòng 7:');print(text[-2000:])
