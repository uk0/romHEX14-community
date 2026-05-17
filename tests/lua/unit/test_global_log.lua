package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

Log("log test message")  -- should not error
Log("test_global_log OK")
