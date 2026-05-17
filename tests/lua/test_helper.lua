-- Sprint L test helper — assertion primitives for unit tests.

function assert_equal(actual, expected, msg)
    if actual ~= expected then
        error("ASSERT FAIL " .. (msg or "") ..
              " expected=" .. tostring(expected) ..
              " got=" .. tostring(actual), 2)
    end
end

function assert_true(v, msg)
    if not v or v == 0 then
        error("ASSERT FAIL " .. (msg or "") .. " expected truthy got=" .. tostring(v), 2)
    end
end

function assert_close(actual, expected, eps, msg)
    eps = eps or 0.001
    if math.abs(actual - expected) > eps then
        error("ASSERT FAIL " .. (msg or "") ..
              " expected~" .. expected ..
              " got=" .. actual ..
              " eps=" .. eps, 2)
    end
end

-- Local-data path helper.  Tests that need real on-disk data
-- (EVC LuaSamples, portal client scripts, customer ROMs) read their
-- location from an environment variable so the test files in the repo
-- stay free of personal paths.  Returns nil when the env var is unset;
-- callers should Log() a skip message and return early.
function env_path(varName, subPath)
    local root = os.getenv(varName)
    if root == nil or root == "" then return nil end
    if subPath == nil or subPath == "" then return root end
    return root .. "/" .. subPath
end
