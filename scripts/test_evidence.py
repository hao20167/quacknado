import pathlib,tempfile,unittest
from evidence import source_hashes
class EvidenceTests(unittest.TestCase):
 def test_inventory_includes_build_definition_and_excludes_runtime_artifacts(self):
  with tempfile.TemporaryDirectory() as d:
   root=pathlib.Path(d)
   for name in ('src/a.cpp','cmake/Core.cmake','CMakeLists.txt','CMakePresets.json','scripts/__pycache__/x.pyc','build/generated.cpp','external/dependency.cpp'):
    p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('fixture')
   a=source_hashes(root);self.assertEqual(set(a),{'src/a.cpp','cmake/Core.cmake','CMakeLists.txt','CMakePresets.json'})
   self.assertEqual(a,source_hashes(root));(root/'cmake/Core.cmake').write_text('changed')
   self.assertNotEqual(a['cmake/Core.cmake'],source_hashes(root)['cmake/Core.cmake'])
if __name__=='__main__':unittest.main()
