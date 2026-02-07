param(
    [Parameter(Mandatory=$true)][string]$ProjectPath,
    [string]$Config = "Release",
    [string]$Platform = "Win64",
    [string]$OutDir = "",
    [string]$RadStudioVersion = "23.0",
    [string]$RsvarsPath = "",
    [bool]$PreClean = $true,
    [bool]$PurgeIntermediates = $true,
    [int]$RetryOnE74 = 1,
    [int]$RetryDelayMs = 1200,
    [int]$LockWaitMs = 5000,
    [bool]$FixIntermediateAcl = $false
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Resolve paths
$projFull = (Resolve-Path $ProjectPath).Path
$projDir = Split-Path -Parent $projFull

if ([string]::IsNullOrWhiteSpace($RsvarsPath)) {
    $RsvarsPath = "C:\Program Files (x86)\Embarcadero\Studio\$RadStudioVersion\bin\rsvars.bat"
}

if (!(Test-Path $RsvarsPath)) {
    throw "rsvars.bat not found at '$RsvarsPath'"
}

# Create output folders
$logs = Join-Path $PSScriptRoot "logs"
$errors = Join-Path $PSScriptRoot "errors"
New-Item -ItemType Directory -Force -Path $logs | Out-Null
New-Item -ItemType Directory -Force -Path $errors | Out-Null
$msbuildLog = Join-Path $logs "msbuild.log"
$msbuildErrorsLog = Join-Path $logs "msbuild.errors.log"

# Remove stale logs from previous runs
if (Test-Path $msbuildLog) { Remove-Item -LiteralPath $msbuildLog -Force -ErrorAction SilentlyContinue }
if (Test-Path $msbuildErrorsLog) { Remove-Item -LiteralPath $msbuildErrorsLog -Force -ErrorAction SilentlyContinue }

# Compose msbuild command line
# Args containing semicolons must be quoted for cmd.exe
$propsStr = "/p:Config=$Config /p:Platform=$Platform"
if ($OutDir -and -not [string]::IsNullOrWhiteSpace($OutDir)) {
    $propsStr += " /p:OutDir=$OutDir"
}

function Invoke-RadMsBuild([string]$Targets, [string]$ExtraArgs = "") {
    $msbuildLine = "msbuild `"$projFull`" $Targets $propsStr /m /v:m"
    if ($ExtraArgs -and -not [string]::IsNullOrWhiteSpace($ExtraArgs)) {
        $msbuildLine += " $ExtraArgs"
    }
    # /bl (binary log) not supported by MSBuild 4.x shipped with RAD Studio
    $msbuildLine += " `"/clp:ErrorsOnly;Summary;DisableConsoleColor`""

    # Run via cmd so rsvars.bat environment applies
    # Use chcp 65001 to keep UTF-8 logs legible
    $cmd = "call `"$RsvarsPath`" && chcp 65001 >NUL && $msbuildLine"
    Write-Host "Invoking: cmd /c $cmd"
    cmd.exe /c $cmd | Out-Host
    return [int]$LASTEXITCODE
}

function Remove-StaleIntermediates([string]$IntermediateDir, [string]$ProjectStem) {
    if (!(Test-Path $IntermediateDir)) {
        return
    }

    $removed = 0
    $failed = @()
    $staleFiles = Get-ChildItem -Path $IntermediateDir -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -like "*.tmp" -or
            $_.Name -like "*.o.tmp" -or
            $_.Name -like "*.obj.tmp"
        }

    foreach ($file in $staleFiles) {
        try {
            if ($file.Attributes -band [IO.FileAttributes]::ReadOnly) {
                $file.Attributes = [IO.FileAttributes]::Archive
            }
            Remove-Item -LiteralPath $file.FullName -Force -ErrorAction Stop
            $removed++
        } catch {
            $failed += $file.FullName
        }
    }

    # Also remove primary object file to avoid rename collisions on E74
    $primaryObj = Join-Path $IntermediateDir "$ProjectStem.o"
    if (Test-Path $primaryObj) {
        try {
            $objInfo = Get-Item -LiteralPath $primaryObj -ErrorAction SilentlyContinue
            if ($objInfo -and ($objInfo.Attributes -band [IO.FileAttributes]::ReadOnly)) {
                $objInfo.Attributes = [IO.FileAttributes]::Archive
            }
            Remove-Item -LiteralPath $primaryObj -Force -ErrorAction Stop
            $removed++
        } catch {
            $failed += $primaryObj
        }
    }

    if ($removed -gt 0) {
        Write-Host "Pre-build cleanup removed $removed intermediate file(s)."
    }
    if ($failed.Count -gt 0) {
        Write-Warning ("Could not remove {0} file(s): {1}" -f $failed.Count, ($failed -join ", "))
    }
}

function Wait-ForUnlockedFile([string]$FilePath, [int]$TimeoutMs) {
    if (!(Test-Path $FilePath)) {
        return $true
    }

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.ElapsedMilliseconds -lt $TimeoutMs) {
        try {
            $stream = [System.IO.File]::Open($FilePath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
            $stream.Close()
            return $true
        } catch {
            Start-Sleep -Milliseconds 200
        }
    }
    return $false
}

function Test-CanDeleteInDirectory([string]$DirectoryPath) {
    if (!(Test-Path $DirectoryPath)) {
        return $true
    }

    # Use a stable probe name to avoid creating many leftovers on denied-delete environments.
    $testFile = Join-Path $DirectoryPath "__delete_probe__.tmp"
    if (!(Test-Path $testFile)) {
        try {
            Set-Content -LiteralPath $testFile -Value "probe" -Encoding UTF8 -ErrorAction Stop
        } catch {
            return $false
        }
    }

    try {
        Remove-Item -LiteralPath $testFile -Force -ErrorAction Stop
        return $true
    } catch {
        return $false
    }
}

function Try-FixIntermediateAcl([string]$DirectoryPath) {
    if (!(Test-Path $DirectoryPath)) {
        return
    }

    $identity = "$env:USERDOMAIN\$env:USERNAME"
    Write-Warning "Trying to grant Modify permission to '$identity' for '$DirectoryPath'..."
    & icacls $DirectoryPath /grant "${identity}:(OI)(CI)M" /T /C | Out-Host
}

$projectStem = [System.IO.Path]::GetFileNameWithoutExtension($projFull)
$intermediateDir = Join-Path $projDir "$Platform\$Config"
$primaryObjPath = Join-Path $intermediateDir "$projectStem.o"

if (-not (Test-CanDeleteInDirectory -DirectoryPath $intermediateDir)) {
    Write-Warning "No delete permission in intermediate directory: $intermediateDir"
    if ($FixIntermediateAcl) {
        Try-FixIntermediateAcl -DirectoryPath $intermediateDir
        if (-not (Test-CanDeleteInDirectory -DirectoryPath $intermediateDir)) {
            Write-Warning "Delete permission is still unavailable after ACL fix attempt."
        }
    } else {
        Write-Warning "Build may fail with E74/MSB3061. Use -FixIntermediateAcl `$true to auto-grant Modify rights."
    }
}

if ($PreClean) {
    if (-not (Wait-ForUnlockedFile -FilePath $primaryObjPath -TimeoutMs $LockWaitMs)) {
        Write-Warning "File appears locked before Clean: $primaryObjPath"
    }

    Write-Host "Running pre-build Clean target..."
    $cleanExit = Invoke-RadMsBuild "/t:Clean"
    if ($cleanExit -ne 0) {
        Write-Warning "Clean target exited with code $cleanExit. Build will continue."
    }
}

if ($PurgeIntermediates) {
    Remove-StaleIntermediates -IntermediateDir $intermediateDir -ProjectStem $projectStem
}

$buildLoggerArgs = "/fileLogger `"/flp:logfile=$msbuildLog;verbosity=diagnostic`""
$buildLoggerArgs += " `"/flp1:logfile=$msbuildErrorsLog;errorsonly`""

$maxAttempts = 1 + [Math]::Max($RetryOnE74, 0)
$attempt = 0
$exitCode = 0

do {
    $attempt++
    Write-Host "Starting build attempt $attempt of $maxAttempts..."
    if (-not (Wait-ForUnlockedFile -FilePath $primaryObjPath -TimeoutMs $LockWaitMs)) {
        Write-Warning "File appears locked before build attempt: $primaryObjPath"
    }
    $exitCode = Invoke-RadMsBuild "/t:Build" $buildLoggerArgs
    if ($exitCode -eq 0) {
        break
    }

    $canRetry = $attempt -lt $maxAttempts
    $isE74 = $false
    if (Test-Path $msbuildErrorsLog) {
        $errorText = Get-Content -LiteralPath $msbuildErrorsLog -Raw -ErrorAction SilentlyContinue
        $isE74 = ($errorText -match "error E74:") -and ($errorText -match "permission denied")
    }

    if (-not ($canRetry -and $isE74)) {
        break
    }

    Write-Warning "Detected E74 permission/rename issue. Cleaning intermediates and retrying..."
    if ($PurgeIntermediates) {
        Remove-StaleIntermediates -IntermediateDir $intermediateDir -ProjectStem $projectStem
    }
    Start-Sleep -Milliseconds $RetryDelayMs
} while ($true)

exit $exitCode
