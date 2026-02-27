#!/usr/bin/env bash
set -euo pipefail

# Wrapper for Git Bash: build via PowerShell, then parse logs into per-file error files
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PROJECT="${1:-./MCPHub.cbproj}"
CONFIG="${CONFIG:-Debug}"
PLATFORM="${PLATFORM:-Win64x}"
OUTDIR_WIN="${OUTDIR_WIN:-}"  # e.g., C:\work\MCPHub\build\

POWERSHELL="powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass"

# Run build (PowerShell script will chain rsvars.bat via cmd.exe)
$POWERSHELL -File "$SCRIPT_DIR/build.ps1" -ProjectPath "$PROJECT" -Config "$CONFIG" -Platform "$PLATFORM" -OutDir "$OUTDIR_WIN"

# Parse errors into individual files + JSON index
python "$SCRIPT_DIR/parse_msbuild_errors.py"
