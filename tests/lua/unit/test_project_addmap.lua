package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectAddMap()
assert_equal(rc, true)
Log("test_project_addmap OK")
