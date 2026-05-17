package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- P0-7: silent-false stubs now populate GetLastError so scripts can
-- detect "feature not present" instead of accepting a quiet `false`.

local function expectStubFailure(name, fn)
    -- Clear previous error.
    GetLastError(true)
    local ok = fn()
    assert_equal(ok, false, name .. " should return false")
    local err = GetLastError(true)
    assert_true(type(err) == "string" and #err > 0,
                name .. " must populate GetLastError on stub failure")
    assert_true(err:lower():find("not implemented") ~= nil,
                name .. " error must mention 'not implemented' (got: "..err..")")
end

expectStubFailure("projectMail",              function() return projectMail() end)
expectStubFailure("projectSetRight",          function() return projectSetRight() end)
expectStubFailure("projectSetRightsOwner",    function() return projectSetRightsOwner() end)
expectStubFailure("projectAutoUpdate",        function() return projectAutoUpdate() end)
expectStubFailure("projectAutoImport",        function() return projectAutoImport() end)
expectStubFailure("projectCloneVehicleData",  function() return projectCloneVehicleData() end)

Log("test_stubs_honest_lasterror OK")
