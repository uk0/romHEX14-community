package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Do NOT actually call (would open browser); just verify function exists.
assert_true(type(StartUrl) == "function")
if false then StartUrl("http://example.invalid/") end   -- protected
Log("test_global_starturl OK")
