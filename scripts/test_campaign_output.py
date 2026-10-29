import pathlib,tempfile,unittest
from campaign_output import campaign_directory
class CampaignOutputTests(unittest.TestCase):
 def test_recorded_data_never_overwritten(self):
  with tempfile.TemporaryDirectory() as d:
   root=pathlib.Path(d)/'campaign';campaign_directory(root)
   p=root/'metadata.json';p.write_text('original')
   with self.assertRaises(ValueError):campaign_directory(root)
   self.assertEqual(p.read_text(),'original')
 def test_prepared_gpu_inputs_allowed(self):
  with tempfile.TemporaryDirectory() as d:
   root=pathlib.Path(d);p=root/'gpu-results.csv';p.write_text('input')
   self.assertEqual(campaign_directory(root),root);self.assertEqual(p.read_text(),'input')
if __name__=='__main__':unittest.main()
