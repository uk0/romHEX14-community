package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

t = timeGetTime()
assert_true(type(t) == "number")
assert_true(t >= 0)
Log("test_global_timegettime OK")
