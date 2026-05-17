package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

o = projectGetElementOffset()
assert_equal(o, 0)
Log("test_project_getelementoffset OK")
