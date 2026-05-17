package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

assert_true(type(GetLastError) == "function")
DuplicateProject("z:\\nonexistent\\file.ols")  -- triggers setLastErr
e = GetLastError()
assert_true(type(e) == "string")
Log("test_global_getlasterror OK")
