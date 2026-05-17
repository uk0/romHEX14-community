package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

arr = FindProjects2(10, "", 0, "", 0, "", ePrjFilename)
assert_true(type(arr) == "table")
Log("test_global_findprojects2 OK")
