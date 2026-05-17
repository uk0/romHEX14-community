package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

HttpStart("http://127.0.0.1:1/never", "GET")
HttpExecute()
body = HttpResponseString()
assert_true(type(body) == "string")
Log("test_global_httpresponsestring OK")
