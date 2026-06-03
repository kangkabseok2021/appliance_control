#Requires -Version 5.1
# Backup-BatchDB.ps1 — Daily PostgreSQL backup for BatchMixerSCADA
# Scheduled via: schtasks /create /tn "BatchMixerDailyBackup"
#               /tr "powershell -NonInteractive -File C:\Scripts\Backup-BatchDB.ps1"
#               /sc DAILY /st 02:00 /ru SYSTEM /rl HIGHEST

$ErrorActionPreference = 'Stop'

$DbUser     = 'batchmixer'
$DbName     = 'batchmixer_prod'
$BackupDir  = 'C:\Backup\BatchMixer'
$RetainDays = 7
$Stamp      = Get-Date -Format 'yyyyMMdd_HHmmss'
$OutFile    = Join-Path $BackupDir "batchmixer_${Stamp}.dump"
$ZipFile    = "${OutFile}.zip"

# Ensure backup directory exists
if (-not (Test-Path $BackupDir)) { New-Item -ItemType Directory -Path $BackupDir | Out-Null }

try {
    # Dump database in custom format
    & pg_dump -U $DbUser -d $DbName --format=custom --file=$OutFile
    if ($LASTEXITCODE -ne 0) { throw "pg_dump exited $LASTEXITCODE" }

    # Compress and remove uncompressed dump
    Compress-Archive -Path $OutFile -DestinationPath $ZipFile -Force
    Remove-Item $OutFile

    # Retain only the most recent N archives
    Get-ChildItem $BackupDir -Filter '*.zip' |
        Sort-Object LastWriteTime -Descending |
        Select-Object -Skip $RetainDays |
        Remove-Item -Force

    Write-EventLog -LogName Application -Source 'BatchMixerSCADA' `
        -EventId 2000 -EntryType Information `
        -Message "Backup SUCCESS: $ZipFile"
}
catch {
    Write-EventLog -LogName Application -Source 'BatchMixerSCADA' `
        -EventId 2001 -EntryType Error `
        -Message "Backup FAILED: $_"
    exit 1
}
