#!/usr/bin/env python3
from campaign_output import prepare_campaign
from evidence import source_hashes
from report_sections import update_section
import collections,csv,hashlib,io,json,pathlib,statistics,subprocess
root=prepare_campaign('docs/benchmarks/ownership')
meta={'campaigns':5,'repetitions':30,'warmup':3,'sha256':source_hashes()}
(root/'metadata.json').write_text(json.dumps(meta,indent=2))
base=list(csv.DictReader(pathlib.Path('docs/benchmarks/render-regression/results.csv').open()))
actual=list(csv.DictReader((root/'gpu-results.csv').open()));assert base==actual and len(actual)==12
log=(root/'gpu-validation.log').read_text()
assert not any(x in log for x in ('runtime error:', 'ERROR: AddressSanitizer','SUMMARY: UndefinedBehaviorSanitizer'))
groups=collections.defaultdict(list)
with (root/'raw.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['campaign','backend','dataset','n','repeat','wall_ns','gpu_ns','checksum'])
 for campaign in range(5):
  for model in ('chicken','creeper','chest'):
   rows=list(csv.DictReader(io.StringIO(subprocess.check_output(['build/perf/Release/quack_bench','600','30',model,'glb'],text=True))))
   w.writerows([campaign]+list(r.values()) for r in rows);f.flush()
   for b in sorted({r['backend'] for r in rows}):groups[b,model].append(statistics.median(float(r['wall_ns']) for r in rows if r['backend']==b))
text='''\n## Vòng 9: null-safe intrusive ownership, GPU sanitizer fail-fast\n
Thay NS::SharedPtr trong source/benchmark/test của project bằng quacknado::metal::Ptr. Adapter chỉ gọi retain/release khi pointer khác null; adopt giữ +1 từ new/alloc, retain dùng cho borrowed autoreleased object. Copy tăng reference, move chuyển quyền và xóa source, assignment dùng copy/move-and-swap để xử lý alias/self-assignment. Không sửa vendored metal-cpp và không thêm sanitizer suppression. Không dùng std::shared_ptr control block hoặc allocation mới cho mỗi resource handle.\n
Test với mock intrusive object kiểm tra null, copy, move, self-copy/self-move, alias assignment, reset và destruction count. GPU regression vẫn so 12 case/36 frame với baseline vòng 8. Lượt GPU chạy ASan/UBSan với halt_on_error=1 cùng Metal API Validation, log không có runtime error/ASan report; pixel/draw counts khớp baseline. Coverage vẫn là ba asset và fixture đã mô tả, không phải chứng minh mọi Metal API.\n
Đo lại GLB 600 object, 5 process/model, 30 frame sau 3 warmup; không diễn giải đây là speedup riêng của ownership adapter. Individual và instanced đều dùng adapter mới.\n
| Model | Submission individual (ms) | Submission instanced (ms) | Ratio |\n|---|---:|---:|---:|\n'''
for model in ('chicken','creeper','chest'):
 a=statistics.median(groups['glb_submit_individual',model]);b=statistics.median(groups['glb_submit_instanced',model]);text+=f'| {model} | {a/1e6:.4f} | {b/1e6:.4f} | {a/b:.2f}× |\n'
text+='''\nRelease và Debug configuration 16/16 CTest qua; GPU fail-fast chạy riêng vì sandbox GPU access. Vẫn có ngoại lệ đã công bố: quack_alloc dùng replacements riêng, không instrument sanitizer. Log và raw ở ownership/. Finding vòng 8 được giữ nguyên như lịch sử, khắc phục ở vòng này.\n
Tái lập GPU check: `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 MTL_DEBUG_LAYER=1 ./build/check/Debug/render_regression build/check/Debug/object.metallib`. Sau khi lưu output vào ownership/gpu-results.csv và stderr vào ownership/gpu-validation.log, chạy `python3 scripts/benchmark_ownership.py` để xác minh baseline và đo lại submission. Không tái dùng kết quả nếu renderer fixture đã đổi mà baseline không còn phù hợp.\n'''
update_section(text,'\n## Vòng 9:');print(text[-1500:])
