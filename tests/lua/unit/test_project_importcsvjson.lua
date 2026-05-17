package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectImportCsvJson("/tmp/x.csv")
assert_equal(rc, false)
Log("test_project_importcsvjson OK")
