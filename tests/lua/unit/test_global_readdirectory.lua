package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

arr = ReadDirectory("tests/lua/unit/*.lua")
assert_true(type(arr) == "table")
assert_true(#arr >= 1, "expected at least 1 lua test file")
Log("test_global_readdirectory OK")
