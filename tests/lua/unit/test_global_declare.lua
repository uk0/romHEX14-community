package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

declare("myDeclaredGlobal", 123)
assert_equal(myDeclaredGlobal, 123)
Log("test_global_declare OK")
