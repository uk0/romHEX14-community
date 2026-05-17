package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectAutoImport()
assert_equal(rc, false)
Log("test_project_autoimport OK")
