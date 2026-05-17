package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

assert_equal(binaryor(1, 2, 4), 7)
assert_equal(binaryor(0xF0, 0x0F), 255)
Log("test_global_binaryor OK")
