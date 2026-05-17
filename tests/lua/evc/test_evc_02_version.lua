package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- EVC sample 02 "Version test" — exercises GetVersion(eWinOLSMajor/Minor).
assert_true(type(GetVersion) == "function")
local major = GetVersion(eWinOLSMajor)
local minor = GetVersion(eWinOLSMinor)
assert_true(type(major) == "number" and major > 0, "major version must be positive")
assert_true(type(minor) == "number" and minor >= 0, "minor version must be non-negative")
Log("test_evc_02_version OK "..major.."."..minor)
