package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Stress: projectSetAt with mode=eSetRelative adds a delta to the existing
-- value (rather than overwriting it).
local addr = 0x101000

-- Anchor value
projectSetAt(addr, 100, eByte)
assert_equal(projectGetAt(addr, eByte), 100, "anchor written")

-- +50 delta
projectSetAt(addr, 50, eByte, eSetRelative)
assert_equal(projectGetAt(addr, eByte), 150, "byte +50 delta")

-- +5 again
projectSetAt(addr, 5, eByte, eSetRelative)
assert_equal(projectGetAt(addr, eByte), 155, "byte +5 delta")

-- Negative delta
projectSetAt(addr, -55, eByte, eSetRelative)
assert_equal(projectGetAt(addr, eByte), 100, "byte -55 delta back to 100")

-- eLoHi relative
projectSetAt(addr, 0x1000, eLoHi)
projectSetAt(addr, 0x0234, eLoHi, eSetRelative)
assert_equal(projectGetAt(addr, eLoHi), 0x1234, "eLoHi +0x0234 delta")

Log("test_stress_setat_relative OK")
