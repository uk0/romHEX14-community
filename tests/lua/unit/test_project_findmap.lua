package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectFindMap("Name", "xyz")
assert_true(type(rc) == "number")
Log("test_project_findmap OK")
