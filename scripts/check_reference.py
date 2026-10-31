#!/usr/bin/env python3
"""Independent Python double-precision prototype vs C++ reference fixtures."""
import csv, io, math, subprocess, sys
binary=sys.argv[1] if len(sys.argv)>1 else 'build/perf/Release/quack_bench'
def sm(a,b,x):
 t=min(1,max(0,(x-a)/(b-a)));return t*t*(3-2*t)
def force(x,y,z,vx,vy,vz):
 z+=8
 if y<0 or y>=30:return (0,0,0)
 u=y/30;r=math.hypot(x,z);t=r/(10+20*u)
 if t>=1.5:return (0,0,0)
 inv=1/r if r>1e-5 else 0
 top=sm(.7,.95,u);side=1-sm(1,1.5,t);ti=1-sm(.95,1,u)
 rotation=50*(t if t<=1 else 1/t)*(1-.5*top)
 radial=(-100*(1-top)+25*top)*min(t,1)
 h=[0,.2,.5,.9,1];s=[.12,.45,1,.25,0]
 lift=0
 for j in range(1,5):
  if u<=h[j]:lift=s[j-1]+(s[j]-s[j-1])*sm(h[j-1],h[j],u);break
 return ((-z*inv*rotation+x*inv*radial-vx)*3*side*ti,math.exp(-t*t)*side*(120*lift-6*vy*ti),(x*inv*rotation+z*inv*radial-vz)*3*side*ti)
raw=subprocess.check_output([binary,'10003','1','mixed','fixtures'],text=True).splitlines()[1:]
maximum=0
for row in csv.DictReader(raw):
 vals=[float(row[k]) for k in ('px','py','pz','vx','vy','vz')];expected=force(*vals)
 for v,k in zip(expected,('fx','fy','fz')):
  actual=float(row[k]);maximum=max(maximum,abs(v-actual));assert abs(v-actual)<=2e-4+2e-5*abs(actual),(row,k,v)
print(f'PASS: Python reference, 10003 samples; max absolute error={maximum:.8g}')
