package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Stress: projectSetAt "?? AA ?? BB" string form — '??' tokens must SKIP
-- the corresponding byte (leave it untouched) while writing the real ones.
local base = 0x104000

-- Seed all four bytes with a marker.
projectSetAt(base + 0, 0x11, eByte)
projectSetAt(base + 1, 0x22, eByte)
projectSetAt(base + 2, 0x33, eByte)
projectSetAt(base + 3, 0x44, eByte)

-- "?? AA ?? BB" should leave bytes 0 and 2 unchanged and overwrite 1 and 3.
projectSetAt(base, "?? AA ?? BB", eByte)
assert_equal(projectGetAt(base + 0, eByte), 0x11, "byte 0 preserved by ??")
assert_equal(projectGetAt(base + 1, eByte), 0xAA, "byte 1 set to AA")
assert_equal(projectGetAt(base + 2, eByte), 0x33, "byte 2 preserved by ??")
assert_equal(projectGetAt(base + 3, eByte), 0xBB, "byte 3 set to BB")

-- Full literal string with no skips
projectSetAt(base, "DE AD BE EF", eByte)
assert_equal(projectGetAt(base + 0, eByte), 0xDE, "byte 0 = DE")
assert_equal(projectGetAt(base + 1, eByte), 0xAD, "byte 1 = AD")
assert_equal(projectGetAt(base + 2, eByte), 0xBE, "byte 2 = BE")
assert_equal(projectGetAt(base + 3, eByte), 0xEF, "byte 3 = EF")

Log("test_stress_setat_skipbytes OK")
