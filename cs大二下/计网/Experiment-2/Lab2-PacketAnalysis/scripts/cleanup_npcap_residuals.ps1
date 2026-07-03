$ErrorActionPreference = "Continue"

$logPath = Join-Path $PSScriptRoot "..\exports\npcap_cleanup.log"
Start-Transcript -LiteralPath $logPath -Force

Write-Host "Removing the known Npcap driver-store package..."
& pnputil.exe /delete-driver oem230.inf /uninstall /force

$serviceKey = "HKLM:\SYSTEM\CurrentControlSet\Services\npcap"
if (Test-Path -LiteralPath $serviceKey) {
    Write-Host "Removing the orphaned Npcap service registry key..."
    Remove-Item -LiteralPath $serviceKey -Recurse -Force
}

$npcapDir = "C:\Program Files\Npcap"
if (Test-Path -LiteralPath $npcapDir) {
    $resolvedNpcapDir = (Resolve-Path -LiteralPath $npcapDir).Path
    if ($resolvedNpcapDir -ne $npcapDir) {
        throw "Refusing to remove unexpected path: $resolvedNpcapDir"
    }

    Write-Host "Removing the failed-install log directory..."
    Remove-Item -LiteralPath $resolvedNpcapDir -Recurse -Force
}

Write-Host "Npcap residual cleanup finished."
Stop-Transcript
