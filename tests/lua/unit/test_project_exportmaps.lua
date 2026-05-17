package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 5 promoted to REAL — wraps existing MapListExporter.
outp = "tests/lua/fixtures/tmp_proj_maps.json"
os.remove(outp)
rc = projectExportMaps(outp)
assert_equal(rc, true)
Log("test_project_exportmaps OK")
