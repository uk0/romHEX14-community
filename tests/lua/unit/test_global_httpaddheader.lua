package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

HttpStart("http://example.invalid/", "GET")
rc = HttpAddHeader("X-Test", "yes")
assert_equal(rc, true)
Log("test_global_httpaddheader OK")
