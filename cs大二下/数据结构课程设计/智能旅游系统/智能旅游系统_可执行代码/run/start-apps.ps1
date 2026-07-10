$ROOT = Split-Path -Parent $PSScriptRoot
$ErrorActionPreference = "Continue"

function Test-PortInUse([int]$Port) {
    return [bool](netstat -ano 2>$null |
        Select-String ":$Port " |
        Select-String "LISTENING" |
        Select-Object -First 1)
}

Write-Host "=== Tourism System - Start Application Services ===" -ForegroundColor Cyan

if (-not (Test-PortInUse 3306)) {
    Write-Host "WARNING: MySQL port 3306 is not listening. Start MySQL first." -ForegroundColor Yellow
}
if (-not (Test-PortInUse 6379)) {
    Write-Host "WARNING: Redis port 6379 is not listening. XHS cache features may degrade." -ForegroundColor Yellow
}

$logDir = Join-Path $ROOT "run"

Write-Host "[1/3] Starting backend on 8080..." -ForegroundColor Green
$backendLog = Join-Path $logDir "backend.log"
Start-Process -FilePath "powershell" `
    -ArgumentList "-NoProfile -Command `"Set-Location '$ROOT'; mvn spring-boot:run -q *>> '$backendLog'`"" `
    -WindowStyle Hidden

Write-Host "[2/3] Starting frontend on 5173..." -ForegroundColor Green
$frontendLog = Join-Path $logDir "frontend.log"
Start-Process -FilePath "powershell" `
    -ArgumentList "-NoProfile -Command `"Set-Location '$ROOT\frontend'; npm run dev *>> '$frontendLog'`"" `
    -WindowStyle Hidden

Write-Host "[3/3] Starting Agent on 9000..." -ForegroundColor Green
$agentLog = Join-Path $logDir "agent.log"
$venvPython = Join-Path $ROOT "agent-service\.venv\Scripts\python.exe"
$python = if (Test-Path $venvPython) { $venvPython } else { "python" }
Start-Process -FilePath "powershell" `
    -ArgumentList "-NoProfile -Command `"Set-Location '$ROOT\agent-service'; & '$python' -m uvicorn app.main:app --host 0.0.0.0 --port 9000 *>> '$agentLog'`"" `
    -WindowStyle Hidden

Write-Host ""
Write-Host "Frontend: http://localhost:5173"
Write-Host "Backend:  http://localhost:8080"
Write-Host "Swagger:  http://localhost:8080/swagger-ui.html"
Write-Host "Agent:    http://localhost:9000"
Write-Host "Logs:     $logDir"
Write-Host "Stop:     .\run\stop-all.ps1"
