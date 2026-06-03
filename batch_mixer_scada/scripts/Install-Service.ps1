#Requires -Version 5.1
#Requires -RunAsAdministrator
# Install-Service.ps1 — Registers BatchMixerSCADA Windows Service
# Usage: .\Install-Service.ps1 -BinPath "C:\BatchMixer\batch_mixer_service.exe"

param(
    [Parameter(Mandatory)]
    [string] $BinPath
)

$ServiceName = 'BatchMixerSCADA'
$DisplayName = 'Batch Mixer SCADA Controller'
$Description = 'Drives the industrial batch mixing plant FSM. Logs to Event Viewer.'

# Register event source (required before first Write-EventLog)
New-EventLog -LogName Application -Source $ServiceName -ErrorAction SilentlyContinue

# Create service
New-Service `
    -Name        $ServiceName `
    -BinaryPathName $BinPath `
    -DisplayName $DisplayName `
    -Description $Description `
    -StartupType Automatic

# Configure failure recovery: restart x2, then reboot
sc.exe failure $ServiceName reset= 86400 `
    actions= restart/5000/restart/5000/reboot/5000

Write-Host "Service '$ServiceName' installed. Start with: sc start $ServiceName"
