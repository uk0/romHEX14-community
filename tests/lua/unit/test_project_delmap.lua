package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectDelMap(0)
assert_true(type(rc) == "number")
Log("test_project_delmap OK")
