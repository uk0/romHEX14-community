package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Stress: projectGetProperty(ePrjPropChecksumMD5, iOrgVer=1) must return the
-- actual MD5 of fixture.bin (computed externally and hardcoded here).
-- fixture.bin is a 2 MB EDC17C46 ECU image — see run_tests.ps1 for how
-- the runner sources it (env $ROMHEX14_FIXTURE_BIN).

local expected_md5 = "f027d0b96f659fa94302f78a32d83628"

-- iOrgVer == 1 -> compute over originalData
local got = projectGetProperty(ePrjPropChecksumMD5, 1)
assert_true(type(got) == "string", "MD5 returns string")
assert_equal(#got, 32, "MD5 hex string is 32 chars")
-- Implementation uses .toHex() which produces lowercase.
assert_equal(string.lower(got), expected_md5, "MD5 matches external hash of fixture.bin")

-- iOrgVer != 1 returns empty per impl
local empty_org = projectGetProperty(ePrjPropChecksumMD5, 0)
assert_equal(empty_org, "", "MD5 with iOrgVer=0 returns empty")

-- Also sanity-check ECU software size while we're here (2 MB).
local sz = projectGetProperty(ePrjPropEcuSoftwaresize)
assert_equal(sz, "2097152", "ECU software size = 2 MB")

Log("test_stress_getproperty_md5 OK")
