package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- versionGetProperty(id) — single arg in our binding (the version is the
-- currently-active project's currentData).  eVerPropChecksum returns
-- MD5 of currentData as 32 lowercase hex chars.
local md5 = versionGetProperty(eVerPropChecksum)
assert_true(type(md5) == "string")
assert_true(#md5 == 32, "expected 32 hex chars, got len="..#md5)
assert_true(md5:match("^[0-9a-f]+$") ~= nil)
Log("test_promote_version_checksum OK md5="..md5)
