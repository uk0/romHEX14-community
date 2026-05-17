package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectAutoUpdate()
assert_equal(rc, false)
Log("test_project_autoupdate OK")
