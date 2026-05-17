package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

arr = FindProjects("", "", "", "", "", 0, "")
assert_true(type(arr) == "table")
Log("test_global_findprojects OK")
