package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

assert_equal(binaryand(3, 7), 3)
assert_equal(binaryand(0xFF, 0x0F), 15)
Log("test_global_binaryand OK")
