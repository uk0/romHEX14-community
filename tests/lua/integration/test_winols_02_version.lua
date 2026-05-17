--------------------------------------------------------------------------------------------
-- version.lua - Display the WinOLS version
--------------------------------------------------------------------------------------------
MessageBox (
   "You are using WinOLS " ..
   GetVersion(eWinOLSMajor) ..
   "." ..
   GetVersion(eWinOLSMinor)
);
