package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

o = fromJSON('{"a":1,"b":"two","c":[1,2,3]}')
assert_equal(o.a, 1)
assert_equal(o.b, "two")
assert_equal(#o.c, 3)
assert_equal(o.c[3], 3)
Log("test_global_fromjson OK")
