package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = DeleteProjectVersion("nonexistent.ols", 0)
assert_equal(rc, false)
Log("test_global_deleteprojectversion OK")
