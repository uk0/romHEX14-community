package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectDelDuplicateMaps(FALSE, TRUE)
assert_true(type(rc) == "number")
Log("test_project_delduplicatemaps OK")
