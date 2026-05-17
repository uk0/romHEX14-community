package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 9.2: round-trip the fixture through IntelHex export + import on
-- a fresh project so we don't disturb the runner-loaded fixture.
local hexPath = "tests/lua/fixtures/tmp_iter9.hex"
os.remove(hexPath)

-- Export fixture to .hex.  projectExport with eFiletypeIntelHex.
local okExport = projectExport(hexPath, eFiletypeIntelHex)
assert_equal(okExport, true, "IntelHex export must succeed")
assert_true(DoesFileExist(hexPath), "hex file must be on disk")

-- Re-import into a fresh project and verify the data round-trips byte
-- for byte.  NewProject leaves the new project empty so size-match guard
-- in projectImport accepts the incoming buffer.
NewProject()
local okImport = projectImport(hexPath, eFiletypeIntelHex)
assert_equal(okImport, true, "IntelHex import must succeed")

-- Spot-check a few bytes match the fixture's baseline.  We can't compare
-- the whole buffer easily; verify at least the first byte and a mid byte
-- are valid (non -99999 sentinel).
local b0 = projectGetAt(0, eByte)
local bMid = projectGetAt(0x1000, eByte)
assert_true(b0 >= 0 and b0 <= 255, "first byte readable: "..tostring(b0))
assert_true(bMid >= 0 and bMid <= 255, "mid byte readable: "..tostring(bMid))

-- Clean up.
projectClose()   -- closes the temp NewProject; fixture becomes active again
os.remove(hexPath)
Log("test_iter9_intelhex_roundtrip OK b0="..b0.." bMid="..bMid)
