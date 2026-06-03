#Requires -Version 5.1
#Requires -RunAsAdministrator
# Uninstall-Service.ps1 — Removes BatchMixerSCADA Windows Service

$ServiceName = 'BatchMixerSCADA'

$svc = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($svc -and $svc.Status -eq 'Running') {
    Stop-Service -Name $ServiceName -Force
    Write-Host "Service stopped."
}

sc.exe delete $ServiceName
Remove-EventLog -Source $ServiceName -ErrorAction SilentlyContinue
Write-Host "Service '$ServiceName' uninstalled."
