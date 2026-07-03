$ErrorActionPreference = "Stop"

$wireshark = "C:\Program Files\Wireshark\Wireshark.exe"
$output = Join-Path $PSScriptRoot "..\screenshots\00_environment\04_wireshark_interface_selection.png"

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class WindowCapture {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int command);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
}
"@

$process = Start-Process -FilePath $wireshark -PassThru
$deadline = (Get-Date).AddSeconds(30)

do {
    Start-Sleep -Milliseconds 500
    $process.Refresh()
} while ($process.MainWindowHandle -eq 0 -and (Get-Date) -lt $deadline)

if ($process.MainWindowHandle -eq 0) {
    throw "Wireshark main window did not appear."
}

[WindowCapture]::ShowWindow($process.MainWindowHandle, 3) | Out-Null
[WindowCapture]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
Start-Sleep -Seconds 5

$rect = New-Object WindowCapture+RECT
if (-not [WindowCapture]::GetWindowRect($process.MainWindowHandle, [ref]$rect)) {
    throw "Could not read the Wireshark window rectangle."
}

$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
$bitmap = New-Object System.Drawing.Bitmap $width, $height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen(
    (New-Object System.Drawing.Point $rect.Left, $rect.Top),
    [System.Drawing.Point]::Empty,
    (New-Object System.Drawing.Size $width, $height)
)

$bitmap.Save($output, [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()

Write-Output $output
