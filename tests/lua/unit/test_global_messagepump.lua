package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

assert_true(type(MessagePump) == "function")
MessagePump()
Log("test_global_messagepump OK")
