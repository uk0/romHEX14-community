package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

ReactivateChecksums()
Log("test_global_reactivatechecksums OK")
