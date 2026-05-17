package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- After projectAddMap, last map exists; query its Name
projectAddMap()
windowSetMapProperties("Name", "TestMap", TRUE)
v = windowGetMapProperties("Name")
assert_equal(v, "TestMap")
Log("test_window_getmapproperties OK")
