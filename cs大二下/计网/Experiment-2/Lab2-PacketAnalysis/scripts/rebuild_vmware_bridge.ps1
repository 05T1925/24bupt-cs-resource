$ErrorActionPreference = "Stop"

$logPath = Join-Path $PSScriptRoot "..\exports\vmware_bridge_rebuild.log"
$uninstallKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{6EC7759F-B700-4743-BB65-BD3ED905D545}"
$displayIcon = (Get-ItemProperty -LiteralPath $uninstallKey).DisplayIcon
$vmwareDir = Split-Path -Parent $displayIcon
$sourceDll = Join-Path $vmwareDir "vmnetBridge.dll"
$sourceInf = Join-Path $vmwareDir "netbridge.inf"
$targetDll = "$env:WINDIR\System32\vmnetbridge.dll"

Start-Transcript -LiteralPath $logPath -Force

if (-not (Test-Path -LiteralPath $sourceDll)) {
    throw "Missing VMware source DLL: $sourceDll"
}
if (-not (Test-Path -LiteralPath $sourceInf)) {
    throw "Missing VMware source INF: $sourceInf"
}

$signature = Get-AuthenticodeSignature -LiteralPath $sourceDll
if ($signature.Status -ne "Valid") {
    throw "VMware bridge DLL signature is not valid: $($signature.Status)"
}

Write-Host "Restoring the signed VMware notify DLL..."
Copy-Item -LiteralPath $sourceDll -Destination $targetDll -Force

Write-Host "Registering the current VMware notify COM server..."
$registerProcess = Start-Process `
    -FilePath "$env:WINDIR\System32\regsvr32.exe" `
    -ArgumentList @("/s", $targetDll) `
    -Wait `
    -PassThru
if ($registerProcess.ExitCode -ne 0) {
    throw "regsvr32 failed with exit code $($registerProcess.ExitCode)"
}

Write-Host "Removing the stale VMware Bridge Protocol component..."
& netcfg.exe -v -u vmware_bridge
if ($LASTEXITCODE -ne 0) {
    throw "netcfg uninstall failed with exit code $LASTEXITCODE"
}

Write-Host "Installing a fresh VMware Bridge Protocol component..."
& netcfg.exe -v -l $sourceInf -c s -i vmware_bridge
if ($LASTEXITCODE -ne 0) {
    throw "netcfg install failed with exit code $LASTEXITCODE"
}

Write-Host "Verifying VMware Bridge Protocol..."
& netcfg.exe -q vmware_bridge

Stop-Transcript
