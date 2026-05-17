package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

v = projectGetChecksumOptionType(0)
assert_equal(v, eCOTNone)
Log("test_project_getchecksumoptiontype OK")
