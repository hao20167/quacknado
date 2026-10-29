#!/usr/bin/env python3
from campaign_output import prepare_campaign
from evidence import source_hashes
from report_sections import update_section
import csv,io,subprocess,pathlib,statistics,collections,json,hashlib
root=prepare_campaign('docs/benchmarks/glb')
(root/'metadata.json').write_text(json.dumps({'campaigns':5,'repetitions':30,'warmup':3,'resolution':'256x256','sha256':source_hashes()},indent=2))
groups=collections.defaultdict(list)
with (root/'raw.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['campaign','backend','dataset','n','repeat','wall_ns','gpu_ns','checksum'])
 for campaign in range(5):
  for model in ('chicken','creeper','chest'):
   for n in (81,600):
    raw=subprocess.check_output(['build/perf/Release/quack_bench',str(n),'30',model,'glb'],text=True)
    rows=list(csv.DictReader(io.StringIO(raw)));w.writerows([campaign]+list(r.values()) for r in rows);f.flush()
    for b in sorted({r['backend'] for r in rows}):groups[b,model,n].append(statistics.median(float(r['wall_ns']) for r in rows if r['backend']==b))
text='''\n## Vòng 6: GLB instancing tích hợp renderer game\n
Application nhóm ModelDraw theo model path. Mỗi primitive của asset giữ vertex/index buffer, texture và material cũ; shader dùng instance_id để đọc orientation/translation/scale riêng. Chia batch tối đa 80 transform × 48 byte = 3.840 byte. setVertexBytes sao chép dữ liệu vào storage của command; không có shared instance buffer bị CPU ghi đè khi GPU còn đọc. Giữ cờ instancing=false để đối chiếu cùng renderer. Hiện pipeline hỗ trợ opaque/alpha-mask, không dùng chiến lược nhóm này cho alpha-blended material cần sort.\n
Benchmark gọi ModelDraw::Initialize/Record và shader thật, dùng chicken/creeper/chest GLB, rotation và vị trí khác nhau, depth test/write và texture. Render offscreen 256×256, kiểm tra ảnh không rỗng và pixel RGBA8 bằng nhau hoàn toàn giữa individual và instanced. 81 object kiểm tra batch tail; 600 object kiểm tra nhiều batch. Năm process/cấu hình, warmup 3, đo 30 frame. Submission và GPU-wait end-to-end được đo riêng; image readback/check nằm ngoài timing.\n
| Metric/backend | Model | N | Median campaign-median | Khoảng campaign |\n|---|---|---:|---:|---:|\n'''
with (root/'summary.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['backend','model','n','median_value','min_value','max_value','unit'])
 for (b,m,n),v in sorted(groups.items()):
  unit='calls' if 'draws' in b else 'ms';scale=1 if unit=='calls' else 1e6
  w.writerow([b,m,n,statistics.median(v)/scale,min(v)/scale,max(v)/scale,unit])
  text+=f'| {b} | {m} | {n} | {statistics.median(v)/scale:.3f} {unit} | {min(v)/scale:.3f}–{max(v)/scale:.3f} |\n'
text+='\nỞ 600 object:\n'
for model in ('chicken','creeper','chest'):
 a=statistics.median(groups['glb_submit_individual',model,600]);b=statistics.median(groups['glb_submit_instanced',model,600]);text+=f'\n- {model}: CPU submission {a/b:.2f}×.\n'
text+='''\nĐây là renderer GLB production được tích hợp vào Application, nhưng phép đo vẫn offscreen chứ chưa phải FPS cửa sổ game. Modes luôn individual trước instanced, do đó còn order/cache bias. Không có scene mixed-model image regression hay camera chuyển động trong fixture hiện tại. Các model không được instantiate ở cùng chỗ để tránh depth-tie tạo khác biệt do đổi primitive order. Benchmark serial wait mỗi frame; lifetime correctness nhiều frame dựa trên copy semantics của setVertexBytes, chưa có stress test nhiều frame-in-flight. Tái lập: build toàn Release để tạo object.metallib, rồi `python3 scripts/benchmark_glb.py` trong môi trường có GPU access.\n'''
update_section(text,'\n## Vòng 6:')
print(text[-1300:])
