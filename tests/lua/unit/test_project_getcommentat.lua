package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

projectAddCommentAt(0x500, 0x500, "get-test")
s = projectGetCommentAt(0x500)
assert_equal(s, "get-test")
Log("test_project_getcommentat OK")
