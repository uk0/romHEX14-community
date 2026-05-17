package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- EVC sample 07 "Two-Step Tuning Server" — uses getversion + analyze
-- modes; needs OpenProjectVersion + GetProjectVersions + similarity.
local samplePath = env_path("ROMHEX14_LUA_SAMPLES",
                            "07 - Two-Step Tuning Server/AdvancedServer.lua")
if samplePath then
    local f, err = loadfile(samplePath)
    assert_true(f ~= nil, "sample 07 must parse: "..tostring(err))
end

-- GetProjectVersions and OpenProjectVersion exist as bindings; the
-- former is REAL (Iter 6), the latter is STUB-FAIL until format-parity
-- work in Iter 9.
assert_true(type(GetProjectVersions)  == "function")
assert_true(type(OpenProjectVersion)  == "function")

-- Until Iter 9 lands, OpenProjectVersion returns false + GetLastError.
local ok = OpenProjectVersion("nonexistent.rx14proj", 0)
assert_equal(ok, false)

-- Similarity is REAL (Iter 6) — sample uses it for analyze mode.
assert_true(type(projectFindSimilarProjectsSql) == "function")

-- Iter 10.8: flags moved out of low-bit collision zone.
assert_equal(eFSPAllowPropertyMatches, 0x10000)
assert_equal(eFSPTrippleRelevance,     0x20000)

Log("test_evc_07_two_step_tuning OK"..(samplePath and "" or " (sample skipped)"))
