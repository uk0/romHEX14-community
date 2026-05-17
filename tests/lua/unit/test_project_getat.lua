package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- First byte of the EDC17C46 fixture must be in range 0-255.
b = projectGetAt(0, eByte)
assert_true(type(b) == "number")
assert_true(b >= 0 and b <= 255, "first byte in range")
arr = projectGetAt(0, eByte, 4)
assert_equal(#arr, 4)
Log("test_project_getat OK")
