#!/usr/bin/env python3
import csv, collections, pathlib, statistics, sys
root=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'docs/benchmarks/results')
groups=collections.defaultdict(list)
for row in csv.DictReader((root/'raw.csv').open()):
 # Placed counts are recorded per step; aggregate by scenario, retaining raw occupancy.
 key=(row['backend'],'scene' if row['backend'] in ('simulation','simulation_reference') else row['dataset'],int(row['n']))
 groups[key].append((float(row['wall_ns']),float(row['gpu_ns'])))
def percentile(x,p):
 x=sorted(x);return x[min(len(x)-1,int((len(x)-1)*p))]
with (root/'summary.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['backend','dataset','n','samples','median_ns','p95_ns','p99_ns','gpu_median_ns'])
 for (b,d,n),values in sorted(groups.items()):
  x=[v[0] for v in values];w.writerow([b,d,n,len(x),statistics.median(x),percentile(x,.95),percentile(x,.99),statistics.median(v[1] for v in values)])
# Standalone SVG, log-scale plot for the mixed kernel scenario.
import math, html
colors=['#888','#167','#c82','#2a7','#a4a','#e54','#159','#964']
backends=sorted({b for b,d,n in groups if d=='mixed'})
svg=['<svg xmlns="http://www.w3.org/2000/svg" width="900" height="520" viewBox="0 0 900 520"><rect width="900" height="520" fill="white"/><text x="50" y="25">Wind latency, mixed dataset (log scales; total CPU↔GPU for GPU)</text>']
all_y=[statistics.median(v[0] for v in vs) for (b,d,n),vs in groups.items() if d=='mixed'];lo=math.floor(math.log10(min(all_y)));hi=math.ceil(math.log10(max(all_y)))
for exp in range(lo,hi+1):
 y=440-(exp-lo)/(hi-lo)*380;svg.append(f'<path d="M70 {y}H680" stroke="#ddd"/><text x="5" y="{y}">10^{exp} ns</text>')
for j,b in enumerate(backends):
 pts=[]
 for n in (1000,10000,100000,1000000):
  key=(b,'mixed',n)
  if key in groups:
   y=440-(math.log10(statistics.median(v[0] for v in groups[key]))-lo)/(hi-lo)*380;x=70+(math.log10(n)-3)*200;pts.append(f'{x},{y}')
 svg.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{colors[j%len(colors)]}" stroke-width="2"/><text x="710" y="{60+j*25}" fill="{colors[j%len(colors)]}">{html.escape(b)}</text>')
for j,n in enumerate(('1k','10k','100k','1M')):svg.append(f'<text x="{70+j*200}" y="475">{n} samples</text>')
svg.append('</svg>');(root/'wind.svg').write_text(''.join(svg))
print(root/'summary.csv')
