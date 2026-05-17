package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Stress projectFindBytes (now REAL) — write a known marker into currentData
-- via projectSetAt, then locate it via projectFindBytes.
projectSetAt(0x10000, "CA FE BA BE", eByte)
hit = projectFindBytes(0, 1, "CA FE BA BE")
assert_equal(hit, 0x10000)

-- Search with wildcards
hit2 = projectFindBytes(0, 1, "CA FE ?? BE")
assert_equal(hit2, 0x10000)

-- StartAddress = -1 returns array of all occurrences
projectSetAt(0x20000, "CA FE BA BE", eByte)
arr = projectFindBytes(-1, 1, "CA FE BA BE")
assert_true(type(arr) == "table")
assert_true(#arr >= 2, "expected at least 2 hits")

Log("test_promote_findbytes_real OK")
