<#
.SYNOPSIS
    Records how Windows enumerates attached serial/USB devices, for Q3-001 / ADR-002.

.DESCRIPTION
    READ-ONLY. Opens no port, sets no baud rate, changes no device state, installs
    nothing and writes nothing outside the -OutFile path. Phase 4 non-product
    verification tooling under directive section 27.

    A no-radio baseline was recorded on this host at 2026-10-06T15:38:08Z:
    zero serial endpoints, and seven USB VID/PID pairs all accounted for as
    laptop-internal or unrelated. Devices in that baseline set are tagged
    [baseline]; anything else is tagged [NEW] so an attached Aerora radio shows
    up as a pure difference.

    Procedure: run once with the radio detached, attach the radio, run again,
    and compare. Report the [NEW] rows verbatim. Do not infer USB identifiers
    from the radio module's silicon - the SX1262/E22 is the radio IC, not
    necessarily the USB interface.

.PARAMETER OutFile
    Optional path to also write the report to. Printed to stdout regardless.

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File .kiro\tools\aerora-radio-observe.ps1
#>
[CmdletBinding()]
param(
    [string] $OutFile
)

Set-StrictMode -Off
$ErrorActionPreference = 'SilentlyContinue'
# Get-PnpDeviceProperty emits progress records that flood non-interactive hosts.
$ProgressPreference = 'SilentlyContinue'

# USB VID:PID pairs observed on this host with NO radio attached.
$BaselineVidPid = @(
    '0BDA:5686',  # Realtek Integrated Webcam HD
    '0A5C:5832',  # Broadcom / Dell ControlVault (composite, smartcard, CVAULT)
    '0CF3:E007',  # Qualcomm QCA61x4A Bluetooth
    '22B8:2E82'   # Motorola moto g34 5G (WPD/MTP)
)

$report = [System.Collections.Generic.List[string]]::new()
function Emit { param([string]$Text) $report.Add($Text) | Out-Null }

Emit ('Aerora radio observation  (READ-ONLY, no port opened)')
Emit ('  observed_utc : ' + (Get-Date).ToUniversalTime().ToString('s') + 'Z')
Emit ('  host         : ' + $env:COMPUTERNAME)
Emit ''

# --- Serial endpoints -------------------------------------------------------
Emit '=== SERIAL ENDPOINTS ==='
$serialFound = $false

$serialComm = Get-ItemProperty -Path 'HKLM:\HARDWARE\DEVICEMAP\SERIALCOMM' -ErrorAction SilentlyContinue
if ($null -eq $serialComm) {
    Emit '  registry SERIALCOMM : key ABSENT (no serial endpoints registered)'
} else {
    foreach ($prop in $serialComm.PSObject.Properties) {
        if ($prop.Name -like 'PS*') { continue }
        $serialFound = $true
        Emit ('  registry SERIALCOMM : ' + $prop.Name + ' = ' + $prop.Value)
    }
}

$portNames = @([System.IO.Ports.SerialPort]::GetPortNames())
$portNamesText = '<none>'
if ($portNames.Count -gt 0) {
    $portNamesText = ($portNames -join ', ')
    $serialFound = $true
}
Emit ('  GetPortNames()      : ' + $portNamesText)

$comEntities = @(Get-CimInstance -ClassName Win32_PnPEntity -ErrorAction SilentlyContinue |
                 Where-Object { $_.Name -match '\(COM\d+\)' })
Emit ('  PnP entities (COMn) : ' + $comEntities.Count)
foreach ($entity in $comEntities) {
    $serialFound = $true
    Emit ('    name         = ' + $entity.Name)
    Emit ('    deviceId     = ' + $entity.DeviceID)
    Emit ('    manufacturer = ' + $entity.Manufacturer)
    Emit ('    service      = ' + $entity.Service)
    Emit ('    status       = ' + $entity.Status)
    Emit ('    hardwareIds  = ' + (@($entity.HardwareID) -join ' | '))
    Emit ('    compatIds    = ' + (@($entity.CompatibleID) -join ' | '))
}

# --- USB identity ----------------------------------------------------------
Emit ''
Emit '=== USB VID/PID (present devices) ==='
$newCount = 0
$usbDevices = @(Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
                Where-Object { $_.InstanceId -match 'USB\\VID_[0-9A-Fa-f]{4}&PID_[0-9A-Fa-f]{4}' })

foreach ($device in $usbDevices) {
    if ($device.InstanceId -notmatch 'VID_([0-9A-Fa-f]{4})&PID_([0-9A-Fa-f]{4})') { continue }
    $pair = ($Matches[1] + ':' + $Matches[2]).ToUpper()
    $isNew = -not ($BaselineVidPid -contains $pair)
    $tag = if ($isNew) { '[NEW]     ' } else { '[baseline]' }
    if ($isNew) { $newCount++ }

    Emit ('  ' + $tag + ' ' + $pair + '  class=' + $device.Class + '  name=' + $device.FriendlyName)
    if (-not $isNew) { continue }

    foreach ($key in @('DEVPKEY_Device_BusReportedDeviceDesc',
                       'DEVPKEY_Device_DriverDesc',
                       'DEVPKEY_Device_DriverProvider',
                       'DEVPKEY_Device_DriverVersion',
                       'DEVPKEY_Device_HardwareIds',
                       'DEVPKEY_Device_CompatibleIds',
                       'DEVPKEY_Device_LocationInfo')) {
        $value = Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName $key -ErrorAction SilentlyContinue
        if ($value -and $null -ne $value.Data -and "$($value.Data)" -ne '') {
            Emit ('               ' + $key.Replace('DEVPKEY_Device_', '') + ' = ' + (@($value.Data) -join ' | '))
        }
    }
    Emit ('               instanceId = ' + $device.InstanceId)
}

# --- Verdict ---------------------------------------------------------------
Emit ''
Emit '=== VERDICT ==='
Emit ('  serial endpoints present : ' + $serialFound)
Emit ('  non-baseline USB devices : ' + $newCount)

if (-not $serialFound -and $newCount -eq 0) {
    Emit '  RESULT: NO RADIO OBSERVED. Matches the recorded no-radio baseline.'
    Emit '          Q3-001 remains OPEN and ADR-002 cannot be confirmed or withdrawn.'
} elseif ($serialFound) {
    Emit '  RESULT: A SERIAL ENDPOINT IS PRESENT. Record the rows above verbatim.'
    Emit '          If it is the Aerora radio, this is decision-tree Case A (standard serial).'
} else {
    Emit '  RESULT: NON-BASELINE USB DEVICE PRESENT, BUT NO SERIAL ENDPOINT.'
    Emit '          Potentially decision-tree Case C. Record the interface before designing anything.'
}

$text = $report -join [Environment]::NewLine
Write-Output $text
if ($OutFile) {
    Set-Content -LiteralPath $OutFile -Value $text -Encoding UTF8
    Write-Output ''
    Write-Output ('written: ' + $OutFile)
}
