package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectReplaceBytes(0, 4095, "AA", "BB")
assert_equal(rc, 0)
Log("test_project_replacebytes OK")
