import csv,json,pathlib,tempfile,unittest
from perf_gate import compare
class GateTests(unittest.TestCase):
 def setUp(self):self.temp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.temp.name)
 def tearDown(self):self.temp.cleanup()
 def campaign(self,name,values):
  root=self.root/name;root.mkdir();(root/'metadata.json').write_text(json.dumps({'environment':{'cpu':'fixture','compiler':'fixture'},'campaigns':5,'repetitions':2,'calibration_target_ns':5000000}))
  with (root/'raw.csv').open('w') as f:
   w=csv.writer(f);w.writerow(['backend','dataset','n','campaign','repeat','wall_ns'])
   for campaign,v in enumerate(values):
    for repeat in range(2):w.writerow(['aos','mixed',1000,campaign,repeat,v])
  with (root/'calibration.csv').open('w') as f:
   w=csv.writer(f);w.writerow(['backend','dataset','n','campaign','iterations_per_batch','calibration_ns'])
   for campaign in range(5):w.writerow(['aos','mixed',1000,campaign,1024,6000000])
  return root
 def pair(self,a=None,b=None):return self.campaign('a',a or [100]*5),self.campaign('b',b or [100]*5)
 def test_same(self):a,b=self.pair();self.assertEqual(compare(a,b)['status'],'PASS')
 def test_known_slowdown(self):a,b=self.pair(b=[130]*5);self.assertEqual(compare(a,b)['status'],'REGRESSION')
 def test_noise(self):a,b=self.pair([50,150,50,150,50],[50,150,50,150,50]);self.assertEqual(compare(a,b)['status'],'INCONCLUSIVE')
 def test_incomplete(self):
  a,b=self.pair();p=b/'raw.csv';p.write_text('\n'.join(p.read_text().splitlines()[:-1])+'\n')
  with self.assertRaises(ValueError):compare(a,b)
 def test_duplicate(self):
  a,b=self.pair();p=b/'raw.csv';p.write_text(p.read_text()+p.read_text().splitlines()[1]+'\n')
  with self.assertRaises(ValueError):compare(a,b)
 def test_nonfinite(self):
  a,b=self.pair(b=[float('nan')]*5)
  with self.assertRaises(ValueError):compare(a,b)
 def test_environment(self):
  a,b=self.pair();p=b/'metadata.json';m=json.loads(p.read_text());m['environment']['cpu']='different';p.write_text(json.dumps(m))
  with self.assertRaises(ValueError):compare(a,b)
 def test_missing_calibration(self):
  a,b=self.pair();(b/'calibration.csv').write_text('backend,dataset,n,campaign,iterations_per_batch,calibration_ns\n')
  with self.assertRaises(ValueError):compare(a,b)
 def test_subthreshold_calibration(self):
  a,b=self.pair();p=b/'calibration.csv';p.write_text(p.read_text().replace('6000000','100'))
  with self.assertRaises(ValueError):compare(a,b)
if __name__=='__main__':unittest.main()
