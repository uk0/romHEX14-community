package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

s = projectGetChecksumName()
assert_equal(s, "")
s2 = projectGetChecksumName(eChecksumNumber)
assert_equal(s2, "")
Log("test_project_getchecksumname OK")
