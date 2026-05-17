package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

v = projectGetChecksumOptionText(0)
assert_equal(v, "")
Log("test_project_getchecksumoptiontext OK")
