package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

assert_true(type(SaveAll) == "function")
SaveAll()  -- no-op in Iter 2
Log("test_global_saveall OK")
