package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 8.1: statChecksums returns -1 ("no engine") instead of 0 ("all good").
rc = projectStatChecksums()
assert_equal(rc, -1)
Log("test_project_statchecksums OK")
