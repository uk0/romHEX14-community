package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

SetClient("test_client_for_get")
c = GetClient()
assert_equal(c, "test_client_for_get")
p = GetClient(true)
last = string.sub(p, -1)
assert_true(last ~= "/" and last ~= "\\", "path must not end with separator")
Log("test_global_getclient OK")
