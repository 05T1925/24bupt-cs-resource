param(
    [int]$DurationSeconds = 1200
)

$ErrorActionPreference = "Stop"

$WorkDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe = Join-Path $WorkDir "datalink.exe"
if (-not (Test-Path -LiteralPath $Exe)) {
    throw "datalink.exe not found: $Exe"
}

$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$OutDir = Join-Path $WorkDir "performance-tests-$Stamp"
New-Item -ItemType Directory -Path $OutDir | Out-Null

$Tests = @(
    @{ Name = "01_plain_ber0";      Args = @("-t", "$DurationSeconds", "-u") },
    @{ Name = "02_plain_ber1e-5";   Args = @("-t", "$DurationSeconds") },
    @{ Name = "03_flood_ber0";      Args = @("-t", "$DurationSeconds", "-f", "-u") },
    @{ Name = "04_flood_ber1e-5";   Args = @("-t", "$DurationSeconds", "-f") },
    @{ Name = "05_flood_ber1e-4";   Args = @("-t", "$DurationSeconds", "-f", "--ber=1e-4") }
)

$FatalPatterns = @(
    "Network Layer received a bad packet",
    "incorrect packet length",
    "Physical Layer Sending Queue overflow",
    "failed",
    "recv_frame\(\): Receiving Queue is empty",
    "get_packet\(\): Network layer is not ready",
    "start_timer\(\): timer",
    "overflow",
    "bad packet"
)

function Read-LastStat {
    param([string]$Path)

    $line = Select-String -LiteralPath $Path -Pattern "packets received" | Select-Object -Last 1
    $quit = [bool](Select-String -LiteralPath $Path -Pattern "Quit\." -Quiet)
    $fatal = Select-String -LiteralPath $Path -Pattern $FatalPatterns -CaseSensitive:$false

    $result = [ordered]@{
        Packets = ""
        Bps = ""
        Utilization = ""
        Errors = ""
        ObservedBer = ""
        LastStat = ""
        Quit = $quit
        FatalMatches = ($fatal | Measure-Object).Count
    }

    if ($line) {
        $result.LastStat = $line.Line
        if ($line.Line -match "([0-9.]+)\s+\.\.\.\.\s+([0-9]+)\s+packets received,\s+([0-9.]+)\s+bps,\s+([0-9.]+)%,\s+Err\s+([0-9]+)\s+\(([^)]+)\)") {
            $result.Packets = $Matches[2]
            $result.Bps = $Matches[3]
            $result.Utilization = $Matches[4]
            $result.Errors = $Matches[5]
            $result.ObservedBer = $Matches[6]
        }
    }

    [pscustomobject]$result
}

$Summary = @()
$Manifest = Join-Path $OutDir "manifest.txt"
"Performance test run: $Stamp" | Set-Content -LiteralPath $Manifest
"Duration per test: $DurationSeconds seconds" | Add-Content -LiteralPath $Manifest
"Executable: $Exe" | Add-Content -LiteralPath $Manifest

$BasePort = Get-Random -Minimum 61000 -Maximum 64000
for ($i = 0; $i -lt $Tests.Count; $i++) {
    $test = $Tests[$i]
    $port = $BasePort + $i
    $testDir = Join-Path $OutDir $test.Name
    New-Item -ItemType Directory -Path $testDir | Out-Null

    $logA = Join-Path $testDir "A.log"
    $logB = Join-Path $testDir "B.log"
    $stdoutA = Join-Path $testDir "A.stdout.txt"
    $stdoutB = Join-Path $testDir "B.stdout.txt"
    $stderrA = Join-Path $testDir "A.stderr.txt"
    $stderrB = Join-Path $testDir "B.stderr.txt"

    $argsA = @($test.Args) + @("-p", "$port", "-l", $logA, "A")
    $argsB = @($test.Args) + @("-p", "$port", "-l", $logB, "B")

    $started = Get-Date
    "[$($started.ToString("s"))] START $($test.Name) port=$port" | Tee-Object -FilePath $Manifest -Append
    "  A: datalink.exe $($argsA -join ' ')" | Add-Content -LiteralPath $Manifest
    "  B: datalink.exe $($argsB -join ' ')" | Add-Content -LiteralPath $Manifest

    $procA = Start-Process -FilePath $Exe -ArgumentList $argsA -WorkingDirectory $WorkDir -RedirectStandardOutput $stdoutA -RedirectStandardError $stderrA -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 1
    $procB = Start-Process -FilePath $Exe -ArgumentList $argsB -WorkingDirectory $WorkDir -RedirectStandardOutput $stdoutB -RedirectStandardError $stderrB -WindowStyle Hidden -PassThru

    $procA.WaitForExit()
    $procB.WaitForExit()
    $procA.Refresh()
    $procB.Refresh()

    $ended = Get-Date
    "[$($ended.ToString("s"))] END   $($test.Name) AExit=$($procA.ExitCode) BExit=$($procB.ExitCode)" | Tee-Object -FilePath $Manifest -Append

    $statA = Read-LastStat -Path $logA
    $statB = Read-LastStat -Path $logB

    $Summary += [pscustomobject]@{
        Test = $test.Name
        Side = "A"
        Port = $port
        Started = $started.ToString("s")
        Ended = $ended.ToString("s")
        ExitCode = $procA.ExitCode
        Packets = $statA.Packets
        Bps = $statA.Bps
        UtilizationPercent = $statA.Utilization
        Errors = $statA.Errors
        ObservedBer = $statA.ObservedBer
        Quit = $statA.Quit
        FatalMatches = $statA.FatalMatches
        Log = $logA
        LastStat = $statA.LastStat
    }
    $Summary += [pscustomobject]@{
        Test = $test.Name
        Side = "B"
        Port = $port
        Started = $started.ToString("s")
        Ended = $ended.ToString("s")
        ExitCode = $procB.ExitCode
        Packets = $statB.Packets
        Bps = $statB.Bps
        UtilizationPercent = $statB.Utilization
        Errors = $statB.Errors
        ObservedBer = $statB.ObservedBer
        Quit = $statB.Quit
        FatalMatches = $statB.FatalMatches
        Log = $logB
        LastStat = $statB.LastStat
    }

    $Summary | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath (Join-Path $OutDir "summary.csv")
}

$Summary | Format-Table -AutoSize | Out-String | Tee-Object -FilePath (Join-Path $OutDir "summary.txt")
"Output directory: $OutDir"
