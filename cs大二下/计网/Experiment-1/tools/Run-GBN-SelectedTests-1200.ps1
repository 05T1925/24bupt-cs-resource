param(
    [int]$DurationSeconds = 1200,
    [string[]]$TestNames = @("01_plain_ber0", "05_flood_ber1e-4"),
    [string]$WorkDir = "C:\Users\28641\Desktop\Experiment-1\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019",
    [string]$OutPrefix = "gbn-selected-tests-1200s"
)

$ErrorActionPreference = "Stop"
$exe = Join-Path $WorkDir "datalink.exe"
if (!(Test-Path $exe)) {
    throw "Executable not found: $exe"
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$outDir = Join-Path $WorkDir "$OutPrefix-$stamp"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$manifest = Join-Path $outDir "manifest.txt"

$allTests = @(
    @{ Name = "01_plain_ber0";    Args = @("-t", "$DurationSeconds", "-u") },
    @{ Name = "05_flood_ber1e-4"; Args = @("-t", "$DurationSeconds", "-f", "--ber=1e-4") }
)

function Get-LastStat {
    param([string]$LogPath)
    if (!(Test-Path $LogPath)) {
        return $null
    }
    $line = Select-String -Path $LogPath -Pattern "packets received" | Select-Object -Last 1
    if ($null -eq $line) {
        return $null
    }
    return $line.Line
}

function Parse-Stat {
    param([string]$Line)
    if ($null -eq $Line) {
        return @{
            Packets = $null; Bps = $null; UtilizationPercent = $null
            Errors = $null; ObservedBer = $null
        }
    }
    if ($Line -match "([0-9]+)\s+packets received,\s+([0-9]+)\s+bps,\s+([0-9.]+)%,\s+Err\s+([0-9]+)\s+\(([^)]+)\)") {
        return @{
            Packets = [int]$Matches[1]
            Bps = [int]$Matches[2]
            UtilizationPercent = [double]$Matches[3]
            Errors = [int]$Matches[4]
            ObservedBer = $Matches[5]
        }
    }
    return @{
        Packets = $null; Bps = $null; UtilizationPercent = $null
        Errors = $null; ObservedBer = $null
    }
}

function Get-FatalMatches {
    param([string]$LogPath)
    if (!(Test-Path $LogPath)) {
        return 0
    }
    return @(Select-String -Path $LogPath -Pattern "FATAL|Abort|bad packet|overflow|assert|Assertion" -CaseSensitive:$false).Count
}

"GBN selected performance tests" | Set-Content -Path $manifest -Encoding UTF8
"Duration per test: $DurationSeconds seconds" | Add-Content -Path $manifest -Encoding UTF8
"Executable: $exe" | Add-Content -Path $manifest -Encoding UTF8
"Started: $(Get-Date -Format o)" | Add-Content -Path $manifest -Encoding UTF8

$basePort = 65000 + (Get-Random -Minimum 0 -Maximum 300)
$rows = New-Object System.Collections.Generic.List[object]
$index = 0

foreach ($test in $allTests) {
    if ($TestNames -notcontains $test.Name) {
        continue
    }

    $port = $basePort + $index
    $index += 1
    $testDir = Join-Path $outDir $test.Name
    New-Item -ItemType Directory -Force -Path $testDir | Out-Null

    $logA = Join-Path $testDir "A.log"
    $logB = Join-Path $testDir "B.log"
    $stdoutA = Join-Path $testDir "A.stdout"
    $stdoutB = Join-Path $testDir "B.stdout"
    $stderrA = Join-Path $testDir "A.stderr"
    $stderrB = Join-Path $testDir "B.stderr"

    $argsA = @("--gbn") + @($test.Args) + @("-p", "$port", "-l", $logA, "A")
    $argsB = @("--gbn") + @($test.Args) + @("-p", "$port", "-l", $logB, "B")

    $started = Get-Date
    Write-Host "[$($started.ToString("s"))] START $($test.Name) port=$port"
    "[$($started.ToString("s"))] START $($test.Name) port=$port" | Add-Content -Path $manifest -Encoding UTF8
    "  A: datalink.exe $($argsA -join ' ')" | Add-Content -Path $manifest -Encoding UTF8
    "  B: datalink.exe $($argsB -join ' ')" | Add-Content -Path $manifest -Encoding UTF8

    $procA = Start-Process -FilePath $exe -ArgumentList $argsA -WorkingDirectory $WorkDir -RedirectStandardOutput $stdoutA -RedirectStandardError $stderrA -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 2
    $procB = Start-Process -FilePath $exe -ArgumentList $argsB -WorkingDirectory $WorkDir -RedirectStandardOutput $stdoutB -RedirectStandardError $stderrB -WindowStyle Hidden -PassThru

    if (-not $procB.HasExited) {
        Wait-Process -InputObject $procB
    }
    if (-not $procA.HasExited) {
        Wait-Process -InputObject $procA
    }
    $ended = Get-Date
    Write-Host "[$($ended.ToString("s"))] END   $($test.Name) AExit=$($procA.ExitCode) BExit=$($procB.ExitCode)"
    "[$($ended.ToString("s"))] END   $($test.Name) AExit=$($procA.ExitCode) BExit=$($procB.ExitCode)" | Add-Content -Path $manifest -Encoding UTF8

    foreach ($side in @("A", "B")) {
        $log = if ($side -eq "A") { $logA } else { $logB }
        $line = Get-LastStat -LogPath $log
        $stat = Parse-Stat -Line $line
        $quit = $false
        if (Test-Path $log) {
            $quit = [bool](Select-String -Path $log -Pattern "Quit\." | Select-Object -First 1)
        }
        $rows.Add([pscustomobject]@{
            Test = $test.Name
            Side = $side
            Port = $port
            Started = $started.ToString("o")
            Ended = $ended.ToString("o")
            Packets = $stat.Packets
            Bps = $stat.Bps
            UtilizationPercent = $stat.UtilizationPercent
            Errors = $stat.Errors
            ObservedBer = $stat.ObservedBer
            Quit = $quit
            FatalMatches = Get-FatalMatches -LogPath $log
            Log = $log
            LastStat = $line
        })
    }
}

"Ended: $(Get-Date -Format o)" | Add-Content -Path $manifest -Encoding UTF8
$rows | Export-Csv -Path (Join-Path $outDir "summary.csv") -NoTypeInformation -Encoding UTF8
$rows | Format-Table -AutoSize | Out-String | Set-Content -Path (Join-Path $outDir "summary.txt") -Encoding UTF8
$rows | Format-Table -AutoSize
Write-Host ""
Write-Host "Output directory: $outDir"
