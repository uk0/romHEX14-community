package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = windowSetActive(1)
assert_equal(rc, true)
Log("test_window_setactive OK")
