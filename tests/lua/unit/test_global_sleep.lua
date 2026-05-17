package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

local t0 = timeGetTime()
Sleep(50)
local elapsed = timeGetTime() - t0
assert_true(elapsed >= 40, "slept at least ~50ms")
Log("test_global_sleep OK")
