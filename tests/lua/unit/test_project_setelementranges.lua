package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectSetElementRanges("Engine / Eprom:0-1048575")
assert_equal(rc, true)
Log("test_project_setelementranges OK")
