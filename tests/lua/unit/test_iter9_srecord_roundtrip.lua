package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 9.3: same as IntelHex test but for Motorola S-record format.
local srecPath = "tests/lua/fixtures/tmp_iter9.s19"
os.remove(srecPath)

local okExport = projectExport(srecPath, eFiletypeMotorolaHex)
assert_equal(okExport, true, "S-record export must succeed")
assert_true(DoesFileExist(srecPath), "S-record file must be on disk")

NewProject()
local okImport = projectImport(srecPath, eFiletypeMotorolaHex)
assert_equal(okImport, true, "S-record import must succeed")

local b0 = projectGetAt(0, eByte)
assert_true(b0 >= 0 and b0 <= 255, "first byte readable: "..tostring(b0))

projectClose()
os.remove(srecPath)
Log("test_iter9_srecord_roundtrip OK b0="..b0)
