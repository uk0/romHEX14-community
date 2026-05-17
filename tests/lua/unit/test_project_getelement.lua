package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

e = projectGetElement()
assert_equal(e, eElementEprom)
Log("test_project_getelement OK")
