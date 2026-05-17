package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- versionSetProperty(id, value) / versionGetProperty(id) — single-version
-- model (active project). eVerStatMaster=7, eVerStatDev=3 per LuaPropertyIds.h.
versionSetProperty(eVerPropState, eVerStatMaster)
local s1 = versionGetProperty(eVerPropState)
assert_equal(tonumber(s1), eVerStatMaster)

versionSetProperty(eVerPropState, eVerStatDev)
local s2 = versionGetProperty(eVerPropState)
assert_equal(tonumber(s2), eVerStatDev)

Log("test_promote_version_state OK")
