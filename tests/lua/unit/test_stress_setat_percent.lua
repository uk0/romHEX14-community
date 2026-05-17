package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Stress: projectSetAt with mode=eSetPercent multiplies existing value by
-- (1 + pct/100). E.g. value=100, pct=20 -> 120.
local addr = 0x102000

projectSetAt(addr, 100, eByte)
assert_equal(projectGetAt(addr, eByte), 100, "anchor 100")

-- +20% -> 120
projectSetAt(addr, 20, eByte, eSetPercent)
assert_equal(projectGetAt(addr, eByte), 120, "byte +20% -> 120")

-- -50% on 120 -> 60
projectSetAt(addr, -50, eByte, eSetPercent)
assert_equal(projectGetAt(addr, eByte), 60, "byte -50% -> 60")

-- 0% is identity
projectSetAt(addr, 50, eByte)
projectSetAt(addr, 0, eByte, eSetPercent)
assert_equal(projectGetAt(addr, eByte), 50, "byte 0% is identity")

-- 16-bit percent
projectSetAt(addr, 1000, eLoHi)
projectSetAt(addr, 10, eLoHi, eSetPercent)
assert_equal(projectGetAt(addr, eLoHi), 1100, "eLoHi +10% -> 1100")

Log("test_stress_setat_percent OK")
