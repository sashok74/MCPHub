param(
    [Parameter(Mandatory=$true)][string]$ProjectPath,
    [string]$Config = "Release",
    [string]$Platform = "Win64",
    [string]$OutDir = "",
    [string]$RadStudioVersion = "37.0",
    [string]$RsvarsPath = ""
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

# Compose msbuild command line
# Args containing semicolons must be quoted for cmd.exe
$targets  = "/t:Build"
$propsStr = "/p:Config=$Config /p:Platform=$Platform"
if ($OutDir -and -not [string]::IsNullOrWhiteSpace($OutDir)) {
    $propsStr += " /p:OutDir=$OutDir"
}

$msbuildLine = "msbuild `"$projFull`" $targets $propsStr /m /v:m"
$msbuildLine += " /fileLogger `"/flp:logfile=$logs\msbuild.log;verbosity=diagnostic`""
$msbuildLine += " `"/flp1:logfile=$logs\msbuild.errors.log;errorsonly`""
$msbuildLine += " `"/clp:ErrorsOnly;Summary;DisableConsoleColor`""

# Build command to run via cmd so rsvars.bat env applies
# Use chcp 65001 to keep UTF-8 logs legible
$cmd = "call `"$RsvarsPath`" && chcp 65001 >NUL && $msbuildLine"

Write-Host "Invoking: cmd /c $cmd"
cmd.exe /c $cmd
exit $LASTEXITCODE
