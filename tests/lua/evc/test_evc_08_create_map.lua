package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- EVC sample 08 "Create a map" — exercises projectAddMap +
-- windowSetMapProperties with the full property bag.  Straight-line
-- sample; we run it end-to-end when $ROMHEX14_LUA_SAMPLES is set,
-- else just verify the API surface (sample is also inlined in
-- tests/lua/integration/test_winols_08_create_map.lua for the
-- always-runs case).
local samplePath = env_path("ROMHEX14_LUA_SAMPLES",
                            "08 - Create a map/create_map.lua")
if not samplePath then
    -- API-only smoke when sample isn't on disk.
    assert_true(type(projectAddMap)              == "function")
    assert_true(type(windowSetMapProperties)     == "function")
    assert_true(type(windowGetMapProperties)     == "function")
    Log("test_evc_08_create_map OK (sample skipped, API-only)")
    return
end

local f, err = loadfile(samplePath)
assert_true(f ~= nil, "sample 08 must parse: "..tostring(err))

-- If the fixture was destroyed by earlier zz_* tests, spin up a fresh
-- project so the sample has something to mutate.
local sz = tonumber(projectGetProperty(ePrjPropEcuSoftwaresize)) or 0
if sz == 0 then
    NewProject()
end

-- Run the sample (it doesn't return — it just executes side effects).
local ok, err2 = pcall(f)
assert_true(ok, "sample 08 must run without error: "..tostring(err2))

-- After running, the last map should be the one named "Kennfeld".
local lastMapName = windowGetMapProperties("Name")
assert_equal(lastMapName, "Kennfeld",
             "last map should be the one sample 08 named 'Kennfeld'")

Log("test_evc_08_create_map OK name="..lastMapName)
