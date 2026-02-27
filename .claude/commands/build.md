---
description: Build the MCPHub C++Builder project, parse errors, report results
argument-hint: [Debug|Release]
allowed-tools: Bash, Read, Glob
---

## Arguments

Build configuration: $ARGUMENTS (default: Debug)

## Instructions

Build the MCPHub C++Builder project. Follow these steps exactly:

### Step 1: Determine configuration

- If the user provided an argument, use it as the Config (Debug or Release)
- Default to Debug if no argument provided
- Platform is always Win64x

### Step 2: Clean old error reports

Delete previous error files so stale results don't persist:

```
powershell -Command "Remove-Item 'C:\RADProjects\MCPHub\doc\cpp_build\errors\*' -Force -ErrorAction SilentlyContinue; Remove-Item 'C:\RADProjects\MCPHub\doc\cpp_build\logs\*' -Force -ErrorAction SilentlyContinue"
```

### Step 3: Run the build

Execute in PowerShell (timeout 5 minutes):

```
powershell -ExecutionPolicy Bypass -File "C:\RADProjects\MCPHub\doc\cpp_build\build.ps1" -ProjectPath "C:\RADProjects\MCPHub\MCPHub.cbproj" -Config <CONFIG> -Platform Win64x
```

Note: the build may return exit code 1 on compilation errors — this is expected, continue to the next step.

### Step 4: Parse errors

Run the error parser:

```
python "C:\RADProjects\MCPHub\doc\cpp_build\parse_msbuild_errors.py"
```

If the parser exits with code 2 ("No logs found"), report that the build produced no log files and stop.

### Step 5: Report results

Read the file `C:\RADProjects\MCPHub\doc\cpp_build\errors\index.json`.

- If `total_issues` is 0: report **Build succeeded** with zero errors.
- If there are errors:
  1. Read each `.err.txt` file from `C:\RADProjects\MCPHub\doc\cpp_build\errors\` directory.
  2. Present errors grouped by file, showing: file path, line number, column, error code, and message.
  3. Read the source files that have errors to understand the context.
  4. Offer to fix the errors.
