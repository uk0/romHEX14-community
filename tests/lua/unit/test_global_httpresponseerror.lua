package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

HttpStart("http://127.0.0.1:1/never", "GET")
HttpExecute()
err = HttpResponseError()
assert_true(type(err) == "string")
Log("test_global_httpresponseerror OK")
