package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectClose()
assert_equal(rc, true)
Log("test_project_close OK")
