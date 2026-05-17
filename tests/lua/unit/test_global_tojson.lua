package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

s = toJSON({n1 = true, n2 = 42})
assert_true(string.find(s, "n2") ~= nil)
Log("test_global_tojson OK")
