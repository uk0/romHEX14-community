# Sprint L test runner — drives rx14 over TCP debug RPC.
#
# Usage: powershell -File tests\lua\run_tests.ps1 [-BuildDir build-rel2]
#
# Steps:
#   1. Build rx14
#   2. Copy fixture EDC17C46 ROM into tests/lua/fixtures/
#   3. Launch rx14 (must have been built with RX14_DEBUG_RPC=ON)
#   4. For each .lua under tests/lua/unit and tests/lua/integration, call
#      `lua_run` over RPC and check the response "ok" field
#   5. Print summary, exit 0 if all passed, 1 otherwise
#
# Tested with PowerShell 5.1 on Windows.

param([string]$BuildDir = "build-rel2")

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $repoRoot

Write-Host "═══ Sprint L test runner ═══"
Write-Host "Repo: $repoRoot"
Write-Host "Build: $BuildDir"

# 1. Build
Write-Host "`n[1/5] Building rx14 …"
& cmake --build $BuildDir --target rx14 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Error "build failed"
    exit 1
}

$exe = Join-Path $BuildDir "rx14.exe"
if (-not (Test-Path $exe)) { Write-Error "missing $exe"; exit 1 }
Write-Host "  exe: $exe ($([math]::Round((Get-Item $exe).Length / 1MB, 1)) MB)"

# 2. Copy fixture (path comes from $env:ROMHEX14_FIXTURE_BIN; if unset
# we fall back to a local fixture.bin already in tests/lua/fixtures/).
Write-Host "`n[2/5] Copying fixture …"
$src = $env:ROMHEX14_FIXTURE_BIN
$dst = "tests\lua\fixtures\fixture.bin"
if ($src -and (Test-Path $src)) {
    Copy-Item -Force $src $dst
    Write-Host "  fixture from `$ROMHEX14_FIXTURE_BIN: $src"
} elseif (Test-Path $dst) {
    Write-Host "  fixture (existing): $dst"
} else {
    Write-Error "no fixture: set `$env:ROMHEX14_FIXTURE_BIN to a 2 MB ECU .bin, or place tests\lua\fixtures\fixture.bin manually"
    exit 1
}
$size = (Get-Item $dst).Length
if ($size -ne 2097152) { Write-Error "fixture size $size != 2 MB"; exit 1 }
Write-Host "  fixture: $dst ($size bytes)"

# 3. Launch rx14 with test mode env var
Write-Host "`n[3/5] Launching rx14 in background …"
$env:RX14_LUA_TEST = "1"
$p = Start-Process -PassThru -WindowStyle Hidden -FilePath $exe
Start-Sleep -Seconds 3

if ($p.HasExited) {
    Write-Error "rx14 exited immediately"
    exit 1
}
Write-Host "  pid: $($p.Id)"

# RPC client helper — TCP JSON-line protocol per src/debug/DebugRpc.cpp
function Send-Rpc {
    param([string]$cmd, [hashtable]$rpcArgs)
    $client = New-Object System.Net.Sockets.TcpClient
    try {
        $client.Connect("127.0.0.1", 48714)
        $stream = $client.GetStream()
        $writer = New-Object System.IO.StreamWriter($stream)
        $reader = New-Object System.IO.StreamReader($stream)
        $body = @{ cmd = $cmd; args = $rpcArgs } | ConvertTo-Json -Compress
        $writer.WriteLine($body)
        $writer.Flush()
        $resp = $reader.ReadLine()
        return $resp | ConvertFrom-Json
    } finally {
        $client.Close()
    }
}

# Wait for RPC to be live (up to 10s)
$tries = 0
$ready = $false
while ($tries -lt 20 -and -not $ready) {
    try {
        $r = Send-Rpc "ping" @{}
        if ($r.ok) { $ready = $true; break }
    } catch {
        Start-Sleep -Milliseconds 500
    }
    $tries++
}
if (-not $ready) {
    Write-Error "rx14 RPC not responding on 127.0.0.1:48714 after 10s"
    Stop-Process -Id $p.Id -Force
    exit 1
}
Write-Host "  RPC alive"

try {
    # 4. Load fixture as ROM (existing load_rom RPC)
    Write-Host "`n[4/5] Loading fixture project …"
    $fixAbs = (Resolve-Path $dst).Path
    $r = Send-Rpc "load_rom" @{ path = $fixAbs }
    if (-not $r.ok) {
        Write-Warning "load_rom failed: $($r.error)"
        # Continue anyway — most Iter 2 globals don't need an open project.
    } else {
        Write-Host "  loaded"
    }

    # 5. Run every .lua under tests/lua/unit + integration + evc
    Write-Host "`n[5/5] Running tests …"
    $tests = Get-ChildItem tests\lua\unit, tests\lua\integration, tests\lua\evc `
                          -Filter "*.lua" -Recurse -ErrorAction SilentlyContinue
    $passed = 0
    $failed = 0
    $failures = @()
    foreach ($t in $tests) {
        $r = Send-Rpc "lua_run" @{ file = $t.FullName }
        if ($r.ok) {
            $passed++
            Write-Host "  PASS $($t.Name)"
        } else {
            $failed++
            $msg = $r.error
            if (-not $msg) { $msg = "(no error msg)" }
            $failures += "$($t.Name): $msg"
            Write-Host "  FAIL $($t.Name): $msg" -ForegroundColor Red
        }
    }

    Write-Host ""
    Write-Host "═══ Summary ═══"
    Write-Host "Passed: $passed"
    Write-Host "Failed: $failed"
    Write-Host "Total:  $($tests.Count)"
    if ($failed -gt 0) {
        Write-Host "`nFailures:" -ForegroundColor Red
        foreach ($f in $failures) { Write-Host "  $f" -ForegroundColor Red }
    }
    exit ([int]($failed -gt 0))
} finally {
    if (-not $p.HasExited) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }
}
