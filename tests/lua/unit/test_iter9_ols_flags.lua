package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 9.6: eIFOls* flags are exposed as constants.  Per-call behaviour
-- needs a real .ols fixture; we only pin the constant values here.
assert_equal(eIFOlsSkipMaps,            0x01)
assert_equal(eIFOlsSkipData,            0x02)
assert_equal(eIFOlsPreserveByteOrder,   0x04)
assert_equal(eIFOlsPreserveBaseAddress, 0x08)

-- Combined via binaryor.
local combined = binaryor(eIFOlsSkipMaps, eIFOlsPreserveByteOrder)
assert_equal(combined, 0x05)

Log("test_iter9_ols_flags OK")
