param(
    [int]$DurationSeconds = 1200,
    [int]$MinLastStatSeconds = 1190,
    [int]$MaxAttempts = 8,
    [string]$WorkDir = "C:\Users\28641\Desktop\Experiment-1\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019",
    [string]$OutPrefix = "gbn-ber1e4-until-complete-1200s"
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $WorkDir "datalink.exe"
if (!(Test-Path $exe)) {
    throw "Executable not found: $exe"
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$rootOut = Join-Path $WorkDir "$OutPrefix-$stamp"
New-Item -ItemType Directory -Force -Path $rootOut | Out-Null
$overall = New-Object System.Collections.Generic.List[object]

function Get-LastStatLine {
    param([string]$LogPath)
    $line = Select-String -Path $LogPath -Pattern "packets received" | Select-Object -Last 1
    if ($null -eq $line) { return $null }
    return $line.Line
}

function Parse-Stat {
    param([string]$Line)
    $result = @{
        Time = $null; Packets = $null; Bps = $null; Utilization = $null; Errors = $null; Ber = $null
    }
    if ($null -eq $Line) { return $result }
    if ($Line -match "^\s*([0-9.]+)\s+\.\.\.\.") {
        $result.Time = [double]$Matches[1]
    }
    if ($Line -match "([0-9]+)\s+packets received,\s+([0-9]+)\s+bps,\s+([0-9.]+)%,\s+Err\s+([0-9]+)\s+\(([^)]+)\)") {
        $result.Packets = [int]$Matches[1]
        $result.Bps = [int]$Matches[2]
        $result.Utilization = [double]$Matches[3]
        $result.Errors = [int]$Matches[4]
        $result.Ber = $Matches[5]
    }
    return $result
}

for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
    $port = 65300 + (Get-Random -Minimum 0 -Maximum 200)
    $attemptDir = Join-Path $rootOut ("attempt-{0:00}" -f $attempt)
    New-Item -ItemType Directory -Force -Path $attemptDir | Out-Null

    $logA = Join-Path $attemptDir "A.log"
    $logB = Join-Path $attemptDir "B.log"
    $stdoutA = Join-Path $attemptDir "A.stdout"
    $stdoutB = Join-Path $attemptDir "B.stdout"
    $stderrA = Join-Path $attemptDir "A.stderr"
    $stderrB = Join-Path $attemptDir "B.stderr"
    $argsA = @("--gbn", "-t", "$DurationSeconds", "-f", "--ber=1e-4", "-p", "$port", "-l", $logA, "A")
    $argsB = @("--gbn", "-t", "$DurationSeconds", "-f", "--ber=1e-4", "-p", "$port", "-l", $logB, "B")

    $started = Get-Date
    Write-Host "[$($started.ToString('s'))] ATTEMPT $attempt START port=$port"
    $procA = Start-Process -FilePath $exe -ArgumentList $argsA -WorkingDirectory $WorkDir -RedirectStandardOutput $stdoutA -RedirectStandardError $stderrA -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 2
    $procB = Start-Process -FilePath $exe -ArgumentList $argsB -WorkingDirectory $WorkDir -RedirectStandardOutput $stdoutB -RedirectStandardError $stderrB -WindowStyle Hidden -PassThru

    if (-not $procB.HasExited) { Wait-Process -InputObject $procB }
    if (-not $procA.HasExited) { Wait-Process -InputObject $procA }
    $ended = Get-Date

    $lineA = Get-LastStatLine $logA
    $lineB = Get-LastStatLine $logB
    $statA = Parse-Stat $lineA
    $statB = Parse-Stat $lineB
    $quitA = [bool](Select-String -Path $logA -Pattern "Quit\." | Select-Object -First 1)
    $quitB = [bool](Select-String -Path $logB -Pattern "Quit\." | Select-Object -First 1)
    $fatalA = @(Select-String -Path $logA -Pattern "FATAL|Abort|bad packet|overflow|assert|Assertion" -CaseSensitive:$false).Count
    $fatalB = @(Select-String -Path $logB -Pattern "FATAL|Abort|bad packet|overflow|assert|Assertion" -CaseSensitive:$false).Count
    $accepted = $quitA -and $quitB -and $fatalA -eq 0 -and $fatalB -eq 0 -and
                $statA.Time -ge $MinLastStatSeconds -and $statB.Time -ge $MinLastStatSeconds

    $overall.Add([pscustomobject]@{
        Attempt = $attempt
        Accepted = $accepted
        Port = $port
        Started = $started.ToString("o")
        Ended = $ended.ToString("o")
        A_LastStatTimeSec = $statA.Time
        A_Packets = $statA.Packets
        A_Bps = $statA.Bps
        A_UtilizationPercent = $statA.Utilization
        A_Errors = $statA.Errors
        A_Quit = $quitA
        A_FatalMatches = $fatalA
        B_LastStatTimeSec = $statB.Time
        B_Packets = $statB.Packets
        B_Bps = $statB.Bps
        B_UtilizationPercent = $statB.Utilization
        B_Errors = $statB.Errors
        B_Quit = $quitB
        B_FatalMatches = $fatalB
        AttemptDir = $attemptDir
        A_LastStat = $lineA
        B_LastStat = $lineB
    })
    $overall | Export-Csv -Path (Join-Path $rootOut "attempts.csv") -NoTypeInformation -Encoding UTF8

    Write-Host "[$($ended.ToString('s'))] ATTEMPT $attempt END accepted=$accepted A_t=$($statA.Time) B_t=$($statB.Time) A_util=$($statA.Utilization) B_util=$($statB.Utilization)"
    if ($accepted) {
        "ACCEPTED=$attempt" | Set-Content -Path (Join-Path $rootOut "accepted.txt") -Encoding UTF8
        $overall | Format-Table -AutoSize
        Write-Host ""
        Write-Host "Accepted directory: $attemptDir"
        Write-Host "Output root: $rootOut"
        exit 0
    }
}

$overall | Format-Table -AutoSize
Write-Host ""
Write-Host "No accepted attempt after $MaxAttempts tries."
Write-Host "Output root: $rootOut"
exit 2
