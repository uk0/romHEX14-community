package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

arr = GetProjectVersions("nonexistent.ols")
assert_true(type(arr) == "table")
Log("test_global_getprojectversions OK")
