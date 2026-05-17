package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

assert_true(type(CloseAll) == "function")
CloseAll()
Log("test_global_closeall OK")
