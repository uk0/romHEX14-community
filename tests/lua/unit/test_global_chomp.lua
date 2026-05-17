package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

assert_equal(chomp("hello\r\n"), "hello")
assert_equal(chomp("hello"), "hello")
assert_equal(chomp(nil), "")
Log("test_global_chomp OK")
