package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- P0-2: HTTP host allowlist gate.
-- The test runner sets RX14_LUA_TEST=1 which bypasses the modal prompt
-- and auto-allows any host; we verify the gate logic by checking that:
-- (a) loopback always allowed,
-- (b) under RX14_LUA_TEST any host returns true,
-- (c) HttpStart returns false / GetLastError populated if gate denies
--     (we can't simulate denial cleanly without the modal, so this
--      branch is covered by inspecting the source — here we just
--      verify the happy paths run.)

-- (a) Loopback host always allowed.
GetLastError(true)
assert_equal(HttpStart("http://127.0.0.1:1/never", "GET"), true,
             "loopback must always pass the gate")

-- (b) Generic external host under test mode auto-allowed.
assert_equal(HttpStart("http://example.invalid/", "GET"), true,
             "RX14_LUA_TEST=1 must auto-allow any host")

-- (c) Malformed input that QUrl can't resolve to a host: the gate refuses
--     (returns false) and HttpStart should NOT crash. QUrl's behaviour
--     here varies by Qt version so we only assert that no crash happens.
local _ = HttpStart("not-a-url-just-a-string", "GET")
local _ = HttpStart("", "GET")

Log("test_http_hostgate OK")
