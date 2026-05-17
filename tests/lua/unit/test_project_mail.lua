package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectMail("me@example.com", "Test", "Body")
assert_equal(rc, false)
Log("test_project_mail OK")
