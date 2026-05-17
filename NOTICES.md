# Third-Party Notices

romHEX14 is distributed under GPL-3.0-or-later. It bundles the following
third-party components under their respective licenses. Full license texts
live alongside each vendored copy.

| Component  | Version | License                       | Location                  | Upstream                                  |
|------------|---------|-------------------------------|---------------------------|-------------------------------------------|
| Lua        | 5.4.8   | MIT                           | `third_party/lua-5.4/`    | https://www.lua.org/                      |
| sol2       | 3.5.0   | MIT                           | `third_party/sol2/`       | https://github.com/ThePhD/sol2            |
| BLAKE3     | portable C reference | CC0-1.0 OR Apache-2.0 | `third_party/blake3/`     | https://github.com/BLAKE3-team/BLAKE3     |

All three licenses are compatible with GPL-3.0-or-later. Sources are
unmodified (with the exception of Lua's `luaconf.h` line endings normalized
for the MinGW build). See each component's `LICENSE` file for the full text
and the original copyright notice.

For the Qt 6 framework linked at build time, see Qt's license terms shipped
with the Qt SDK (LGPL-3.0 or commercial).
