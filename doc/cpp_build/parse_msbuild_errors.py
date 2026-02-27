#!/usr/bin/env python3
# Parses msbuild.log and/or msbuild.errors.log into per-file error text files and a JSON index
import re, os, json, sys

ROOT = os.path.dirname(os.path.abspath(__file__))
LOGS = os.path.join(ROOT, "logs")
ERRORS = os.path.join(ROOT, "errors")
os.makedirs(ERRORS, exist_ok=True)

# Prefer errors-only log if present
candidates = [os.path.join(LOGS, "msbuild.errors.log"), os.path.join(LOGS, "msbuild.log")]
log_path = next((p for p in candidates if os.path.exists(p)), None)
if not log_path:
    print("No logs found to parse.", file=sys.stderr)
    sys.exit(2)

with open(log_path, "r", encoding="utf-8", errors="replace") as f:
    lines = f.read().splitlines()

# Typical MSBuild/Embarcadero formats:
# File.cpp(123): error E2451: message
# File.cpp(123,45): warning W8004: message
# Path\to\File.cpp(12,7): error: something (some tools omit code)
# 1>uMain.cpp(15,13): error E4656: message [C:\path\project.cbproj]
pattern = re.compile(r"""^
    (?:\d+>)?                                       # optional MSBuild node prefix "1>"
    (?P<file>[A-Za-z]:\\[^:(]+|\.[^:(]+|[^:(]+)     # file path (rough)
    \(
        (?P<line>\d+)
        (?:,(?P<col>\d+))?
    \)\s*:\s*
    (?:(?P<kind>error|warning))\s*
    (?P<code>[A-Za-z]\d+)?
    \s*:\s*
    (?P<message>.*)
$""", re.X)

by_file = {}
all_issues = []

for ln in lines:
    m = pattern.match(ln.strip())
    if not m:
        continue
    d = m.groupdict()
    file_path = d["file"].strip()
    # Normalize path separators for Windows style
    file_path = file_path.replace("/", "\\")
    item = {
        "file": file_path,
        "line": int(d["line"]),
        "col": int(d["col"]) if d.get("col") else None,
        "kind": d.get("kind") or "error",
        "code": (d.get("code") or "").strip(),
        # Strip trailing MSBuild project reference like [C:\path\project.cbproj]
        "message": re.sub(r'\s*\[.*\.cbproj\]\s*$', '', d.get("message") or "")
    }
    all_issues.append(item)
    by_file.setdefault(file_path, []).append(item)

def safe_name(p):
    # Turn "C:\path\to\File.cpp" into "C__path_to_File.cpp.err.txt"
    s = p.replace(":", "_").replace("\\", "_").replace("/", "_")
    return s

# Write per-file error reports
for fpath, issues in by_file.items():
    name = safe_name(fpath) + ".err.txt"
    outp = os.path.join(ERRORS, name)
    with open(outp, "w", encoding="utf-8") as w:
        for it in issues:
            loc = f"{it['file']}({it['line']}{','+str(it['col']) if it['col'] else ''})"
            code = f" {it['code']}" if it['code'] else ""
            w.write(f"{loc}: {it['kind']}{code}: {it['message']}\n")

# Write JSON index for agents
index = {
    "total_issues": len(all_issues),
    "files_with_issues": len(by_file),
    "by_file": by_file
}
with open(os.path.join(ERRORS, "index.json"), "w", encoding="utf-8") as w:
    json.dump(index, w, ensure_ascii=False, indent=2)

print(f"Parsed {len(all_issues)} issues across {len(by_file)} files.")
