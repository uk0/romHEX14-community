package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

assert_equal(binaryxor(3, 4), 7)
assert_equal(binaryxor(0xFF, 0xFF), 0)
Log("test_global_binaryxor OK")
