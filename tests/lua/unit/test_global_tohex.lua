package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

assert_equal(tohex(256), "100")
assert_equal(tohex(255), "FF")
assert_equal(tohex(0),   "0")
Log("test_global_tohex OK")
