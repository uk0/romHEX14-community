package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 10.6: projectFindBytes/ReplaceBytes accept a trailing datatype that
-- encodes numeric pattern values per the WinOLS endianness.

-- Plant a known 16-bit LoHi pattern at a fresh address so we don't disturb
-- the fixture's real ROM data permanently.
local addr = 0x500
local oldLo = projectGetAt(addr,     eByte)
local oldHi = projectGetAt(addr + 1, eByte)
-- Write 0x4D2 as Lo/Hi: bytes [0xD2, 0x04].
projectSetAt(addr, 0x4D2, eLoHi)

-- Find as LoHi number: should locate byte addr.
local hit = projectFindBytes(0, 1 --[[orgVer=1=currentData]], 0x4D2, eLoHi)
assert_equal(hit, addr, "LoHi search must find planted pattern")

-- Same number searched as HiLo should NOT find it (different byte order).
local hitWrong = projectFindBytes(0, 1, 0x4D2, eHiLo)
assert_true(hitWrong < 0 or hitWrong ~= addr,
            "HiLo search must not match a LoHi-encoded value at same addr")

-- Replace via projectReplaceBytes with datatype encoding.  Bring it back
-- to original.
local n = projectReplaceBytes(0, 0x1000, 0x4D2, oldLo + oldHi*256,
                              1 --[[orgVer]], 1 --[[count]], eLoHi)
assert_equal(n, 1, "replace must rewrite exactly one occurrence")

-- Verify the bytes were rewritten to the original (re-encoded).
local b0 = projectGetAt(addr,     eByte)
local b1 = projectGetAt(addr + 1, eByte)
assert_equal(b0, oldLo, "byte 0 restored")
assert_equal(b1, oldHi, "byte 1 restored")
Log("test_iter10_findbytes_datatype OK")
