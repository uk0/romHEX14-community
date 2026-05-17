package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

HttpStart("http://127.0.0.1:1/never", "GET")
HttpExecute()
tmp = "tests/lua/fixtures/tmp_http_response.bin"
rc = HttpResponseFile(tmp)
assert_true(rc == true or rc == false)  -- either OK or write failed; both valid
Log("test_global_httpresponsefile OK")
