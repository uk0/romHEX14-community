package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

HttpStart("http://example.invalid/", "GET")
rc = HttpAddParam("k", "v")
assert_equal(rc, true)
Log("test_global_httpaddparam OK")
