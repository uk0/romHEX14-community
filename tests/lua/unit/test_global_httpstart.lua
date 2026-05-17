package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- HttpStart just initializes the request buffer.
assert_true(type(HttpStart) == "function")
rc = HttpStart("http://127.0.0.1:1/never", "GET")
assert_equal(rc, true)
Log("test_global_httpstart OK")
