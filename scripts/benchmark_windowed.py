#!/usr/bin/env python3
from campaign_output import prepare_campaign
from evidence import source_hashes
from report_sections import update_section
import collections,csv,hashlib,io,json,os,pathlib,random,statistics,subprocess
root=prepare_campaign('docs/benchmarks/windowed')
(root/'metadata.json').write_text(json.dumps({'campaigns':3,'frames':210,'discard_first':30,'seed':42,'dt':'1/60','sha256':source_hashes()},indent=2))
groups=collections.defaultdict(list)
with (root/'raw.csv').open('w') as f:
 w=None
 for campaign in range(3):
  modes=['individual','instanced'];random.Random(42+campaign).shuffle(modes);states={}
  for mode in modes:
   env=os.environ|{'QUACK_REPLAY_FRAMES':'210','QUACK_REPLAY_MODE':mode,'QUACK_SCENE_COUNT':'600','QUACK_REPLAY_CLOCK':'fixed','QUACK_REPLAY_WIND':'1','QUACK_PHYSICS_WORKERS':'0','QUACK_PHYSICS_QOS':'0'}
   p=subprocess.run(['build/perf/Release/quacknado'],env=env,text=True,capture_output=True,check=False,timeout=60)
   (root/f'{campaign}-{mode}.log').write_text(p.stderr)
   if p.returncode:
    (root/f'{campaign}-{mode}-failed.csv').write_text(p.stdout)
    p.check_returncode()
   rows=list(csv.DictReader(io.StringIO(p.stdout)));assert len(rows)==210 and all(r['mode']==mode and int(r['frame'])==i for i,r in enumerate(rows))
   states[mode]=[(r['checksum'],r['active'],r['width'],r['height']) for r in rows]
   if w is None:w=csv.writer(f);w.writerow(['campaign']+list(rows[0]))
   w.writerows([campaign]+list(r.values()) for r in rows);f.flush()
   for metric in ('simulation_ns','submission_ns','render_call_ns','cpu_frame_ns','gpu_command_ns'):
    values=sorted(float(r[metric]) for r in rows[30:]);assert min(values)>=0
    if metric=='gpu_command_ns':assert min(values)>0
    groups[mode,metric].append((statistics.median(values),values[int(.99*(len(values)-1))]))
  assert states['individual']==states['instanced'],'Replay physics/resolution mismatch'
text='''\n## Vòng 11: windowed replay trên game thật\n
Thêm QUACK_REPLAY_FRAMES (1..2000) và QUACK_REPLAY_MODE=individual/instanced. Chỉ khi replay được bật, scene dùng seed 42, 600 object, đúng một Update dt=1/60 mỗi frame, tornado đổi chiều X mỗi 60 step; input keyboard không điều khiển replay. Replay tự kết thúc, không ghi I/O trong vòng đo. Normal interactive loop vẫn dùng accumulator. Individual giữ thứ tự module/object gốc; instanced dùng grouping production.\n
Renderer giữ tối đa ba command đã submit trước khi retire command cũ, thu GPU timestamp sau completion, không completion callback truy cập dữ liệu đã hủy. CPU submission đo sau nextDrawable đến commit, không gồm chờ drawable; Render call gồm drawable wait và retire wait. CPU frame gồm poll events + simulation + Render; checksum/telemetry nằm ngoài vùng đó. FinishProfile drain các command còn lại trước khi xuất CSV; thời gian drain cuối run không thuộc từng frame. Đây là replay presentation-paced, không dùng tổng runtime làm FPS tương tác hay throughput không giới hạn.\n
Ba campaign/process mỗi mode, shuffle thứ tự, 210 frames/run, bỏ 30 đầu; raw giữ physical pixel size, active bodies và checksum. Mỗi cặp mode phải khớp trajectory/checksum/active/resolution. Chưa có screenshot regression toàn scene; kiểm tra ảnh vòng 8/9 vẫn là fixture riêng.\n
| Mode | Metric | Median của campaign median (ms) | Median của campaign p99 (ms) |\n|---|---|---:|---:|\n'''
with (root/'summary.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['mode','metric','median_ns','median_campaign_p99_ns'])
 for (mode,metric),values in sorted(groups.items()):
  median=statistics.median(v[0] for v in values);p99=statistics.median(v[1] for v in values);w.writerow([mode,metric,median,p99]);text+=f'| {mode} | {metric} | {median/1e6:.3f} | {p99/1e6:.3f} |\n'
text+='''\nP99 từ 180 frame/campaign chỉ là thống kê mô tả ngắn, chưa đủ mô tả rare tail. Window compositor, refresh rate, focus/visibility, power và GPU load không được khóa. NextDrawable/presentation có thể che bớt lợi ích submission; không suy speedup từ microbenchmark thành FPS. Profiling có overhead và bookkeeping; bộ đệm timing được reserve trước run nhưng đây không phải zero-allocation gate. Tái lập: build toàn game Release, chạy `python3 scripts/benchmark_windowed.py` trong desktop session có GPU/window access; cửa sổ mở và tự đóng cho từng process. Không tự bỏ qua lỗi drawable/command hoặc người dùng đóng cửa sổ giữa replay.\n'''
update_section(text,'\n## Vòng 11:');print(text[-1600:])
