package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Stress: write a known value with projectSetAt for EVERY datatype,
-- then read it back with projectGetAt and assert exact equality.
-- Uses an address well inside the 2MB EDC17C46 fixture.
local base = 0x100000  -- 1 MB in, well inside the 2 MB ROM, 4-byte aligned

-- eByte
projectSetAt(base, 0xA5, eByte)
assert_equal(projectGetAt(base, eByte), 0xA5, "eByte round-trip")

-- eLoHi (little-endian 16)
projectSetAt(base, 0x1234, eLoHi)
assert_equal(projectGetAt(base, eLoHi), 0x1234, "eLoHi round-trip")
-- Byte order: low byte first
assert_equal(projectGetAt(base, eByte),     0x34, "eLoHi low byte at base")
assert_equal(projectGetAt(base + 1, eByte), 0x12, "eLoHi high byte at base+1")

-- eHiLo (big-endian 16)
projectSetAt(base, 0xABCD, eHiLo)
assert_equal(projectGetAt(base, eHiLo), 0xABCD, "eHiLo round-trip")
assert_equal(projectGetAt(base, eByte),     0xAB, "eHiLo high byte at base")
assert_equal(projectGetAt(base + 1, eByte), 0xCD, "eHiLo low byte at base+1")

-- eLoHiLoHi (little-endian 32)
projectSetAt(base, 0xDEADBEEF, eLoHiLoHi)
assert_equal(projectGetAt(base, eLoHiLoHi), 0xDEADBEEF, "eLoHiLoHi round-trip")
assert_equal(projectGetAt(base, eByte),     0xEF, "eLoHiLoHi byte 0")
assert_equal(projectGetAt(base + 3, eByte), 0xDE, "eLoHiLoHi byte 3")

-- eHiLoHiLo (big-endian 32)
projectSetAt(base, 0xCAFEBABE, eHiLoHiLo)
assert_equal(projectGetAt(base, eHiLoHiLo), 0xCAFEBABE, "eHiLoHiLo round-trip")
assert_equal(projectGetAt(base, eByte),     0xCA, "eHiLoHiLo byte 0")
assert_equal(projectGetAt(base + 3, eByte), 0xBE, "eHiLoHiLo byte 3")

-- eFloatLoHi (IEEE 754 32-bit little-endian)
projectSetAt(base, 3.14, eFloatLoHi)
assert_close(projectGetAt(base, eFloatLoHi), 3.14, 0.0001, "eFloatLoHi round-trip")

-- eFloatHiLo (IEEE 754 32-bit big-endian)
projectSetAt(base, -2.5, eFloatHiLo)
assert_close(projectGetAt(base, eFloatHiLo), -2.5, 0.0001, "eFloatHiLo round-trip")

Log("test_stress_getat_dtypes OK")
