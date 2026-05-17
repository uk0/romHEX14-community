package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = SetClient("test_client_lua")
assert_true(rc == true or rc == nil)
Log("test_global_setclient OK")
