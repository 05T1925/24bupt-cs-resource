$ErrorActionPreference = "Continue"

$logPath = Join-Path $PSScriptRoot "..\exports\vmware_bridge_cleanup.log"
Start-Transcript -LiteralPath $logPath -Force

Write-Host "Disabling orphaned VMware Bridge Protocol bindings..."
Get-NetAdapterBinding -ComponentID "vmware_bridge" -ErrorAction SilentlyContinue |
    Where-Object { $_.Enabled } |
    ForEach-Object {
        Write-Host "Disabling vmware_bridge on adapter: $($_.Name)"
        Disable-NetAdapterBinding -Name $_.Name -ComponentID "vmware_bridge" -Confirm:$false
    }

Write-Host "Removing the orphaned vmware_bridge network component..."
& netcfg.exe -v -u vmware_bridge

Write-Host "Verifying component state..."
& netcfg.exe -q vmware_bridge

Stop-Transcript
