package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

assert_equal(fromhex("100"), 256)
assert_equal(fromhex("FF"),  255)
assert_equal(fromhex("0"),   0)
assert_equal(fromhex("DEAD"), 57005)
Log("test_global_fromhex OK")
