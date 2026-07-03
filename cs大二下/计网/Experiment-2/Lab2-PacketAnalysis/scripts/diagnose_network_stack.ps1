$ErrorActionPreference = "Continue"

$outputDir = Join-Path $PSScriptRoot "..\exports"
$outputPath = Join-Path $outputDir "network_stack_diagnostics.txt"

Start-Transcript -LiteralPath $outputPath -Force

Write-Host "=== Windows version ==="
cmd.exe /c ver
Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion" |
    Select-Object ProductName, DisplayVersion, CurrentBuild, UBR

Write-Host "=== Base Filtering Engine ==="
Get-Service -Name BFE | Format-List Name, Status, StartType

Write-Host "=== Network adapters ==="
Get-NetAdapter -IncludeHidden |
    Select-Object Name, InterfaceDescription, Status, MacAddress, LinkSpeed |
    Format-Table -AutoSize -Wrap

Write-Host "=== Enabled non-Microsoft-prefixed bindings ==="
Get-NetAdapterBinding |
    Where-Object { $_.Enabled -and $_.ComponentID -notmatch "^ms_" } |
    Select-Object Name, DisplayName, ComponentID, Enabled |
    Format-Table -AutoSize -Wrap

Write-Host "=== All network components (netcfg -s n) ==="
& netcfg.exe -s n

Write-Host "=== Network adapters known to netcfg (netcfg -s a) ==="
& netcfg.exe -s a

Write-Host "=== Binding map ==="
Push-Location $outputDir
& netcfg.exe -v -m
Pop-Location

Write-Host "=== Npcap-related driver packages ==="
& pnputil.exe /enum-drivers |
    Select-String -Pattern "Npcap|Nmap|WinPcap" -Context 4,8

Stop-Transcript
