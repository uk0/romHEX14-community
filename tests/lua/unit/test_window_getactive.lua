package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

id = windowGetActive()
assert_true(type(id) == "number")
Log("test_window_getactive OK")
