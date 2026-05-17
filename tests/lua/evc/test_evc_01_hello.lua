package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- EVC sample 01 "Hello World" — verifies MessageBox is callable.
-- In RX14_LUA_TEST=1 mode our MessageBox just appends to engine output
-- (no actual modal), so the sample's single call is a smoke test that
-- the global is bound and accepts a string.
assert_true(type(MessageBox) == "function")
local rc = MessageBox("Hello World!")
assert_equal(rc, 1, "MessageBox should return IDOK in test mode")
Log("test_evc_01_hello OK")
