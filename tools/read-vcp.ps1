<#
.SYNOPSIS
    Read firmware log lines from the ST-LINK virtual COM port.

.DESCRIPTION
    Opens the VCP (115200 8N1), optionally resets the board through the
    ST-LINK so the boot log is captured from the first line, and prints
    every received line until -Until matches or -Seconds elapses.
    Exit code: 0 if -Until matched (or no -Until given), 1 on timeout.

.EXAMPLE
    tools\read-vcp.ps1 -Port COM4 -Reset -Until '^APP READY'
#>
param(
    [string]$Port = 'COM4',
    [int]$Baud = 115200,
    [int]$Seconds = 15,
    [string]$Until = '',
    [switch]$Reset,
    [string]$Programmer = "$env:LOCALAPPDATA\stm32cube\bundles\programmer\2.23.0\bin\STM32_Programmer_CLI.exe"
)

$ErrorActionPreference = 'Stop'

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
$sp.NewLine = "`r`n"
$sp.ReadTimeout = 200
$sp.Open()
try {
    $sp.DiscardInBuffer()
    if ($Reset) {
        & $Programmer -c port=SWD mode=HOTPLUG -rst | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "STM32_Programmer_CLI reset failed ($LASTEXITCODE)" }
    }

    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $line = $sp.ReadLine()
        } catch [System.TimeoutException] {
            continue
        }
        Write-Output $line
        if ($Until -and ($line -match $Until)) { exit 0 }
    }
    if ($Until) {
        Write-Error "timeout after $Seconds s waiting for '$Until'" -ErrorAction Continue
        exit 1
    }
} finally {
    $sp.Close()
}
