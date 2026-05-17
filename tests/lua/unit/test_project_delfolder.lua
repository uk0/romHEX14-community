package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectDelFolder("My maps")
assert_true(type(rc) == "boolean")
Log("test_project_delfolder OK")
