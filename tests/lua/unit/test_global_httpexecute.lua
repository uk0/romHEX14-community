package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- We do NOT want to hit a real network during tests. Use an unroutable
-- address so HttpExecute returns quickly with an error. Verify the
-- function returns a numeric status (0 on connection failure).
HttpStart("http://127.0.0.1:1/never", "GET")
status = HttpExecute()
assert_true(type(status) == "number")
Log("test_global_httpexecute OK (status=" .. tostring(status) .. ")")
