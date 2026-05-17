package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectImportFromOls("nonexistent.ols", 0, 0)
assert_equal(rc, false)
Log("test_project_importfromols OK")
