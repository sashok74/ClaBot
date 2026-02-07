#!/usr/bin/env python3
# Parses msbuild.log and/or msbuild.errors.log into per-file reports and JSON index
import json
import os
import re
import sys

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

# With location:
# 1>uMain.cpp(15,13): error E4656: ...
# 1>C:\Program Files (x86)\...\CodeGear.Cpp.Targets(3018,5): error E74: ...
WITH_LOCATION = re.compile(
    r"^(?:\d+>)?(?P<file>.+?)\((?P<line>\d+)(?:,(?P<col>\d+))?\)\s*:\s*"
    r"(?P<kind>error|warning)\s*(?P<code>[A-Za-z]+\d+)?\s*:\s*(?P<message>.+)$",
    re.IGNORECASE,
)

# Without location:
# some_tool: error XYZ123: ...
WITHOUT_LOCATION = re.compile(
    r"^(?:\d+>)?(?P<file>.+?)\s*:\s*(?P<kind>error|warning)\s*"
    r"(?P<code>[A-Za-z]+\d+)?\s*:\s*(?P<message>.+)$",
    re.IGNORECASE,
)


def normalize_file_path(file_path: str) -> str:
    file_path = file_path.strip().strip('"')
    return file_path.replace("/", "\\")


def normalize_message(message: str) -> str:
    # Strip trailing project marker: [C:\path\project.cbproj]
    return re.sub(r"\s*\[.*?\.cbproj\]\s*$", "", (message or "").strip())


def to_issue(match: re.Match, has_location: bool) -> dict:
    d = match.groupdict()
    issue = {
        "file": normalize_file_path(d["file"]),
        "line": int(d["line"]) if has_location else None,
        "col": int(d["col"]) if has_location and d.get("col") else None,
        "kind": (d.get("kind") or "error").lower(),
        "code": (d.get("code") or "").strip(),
        "message": normalize_message(d.get("message") or ""),
    }
    return issue


by_file = {}
all_issues = []

for raw_line in lines:
    line = raw_line.strip()
    if not line:
        continue

    match = WITH_LOCATION.match(line)
    has_location = True

    if not match:
        match = WITHOUT_LOCATION.match(line)
        has_location = False

    if not match:
        continue

    issue = to_issue(match, has_location)
    all_issues.append(issue)
    by_file.setdefault(issue["file"], []).append(issue)


def safe_name(path: str) -> str:
    # Turn "C:\path\to\File.cpp" into "C__path_to_File.cpp"
    return path.replace(":", "_").replace("\\", "_").replace("/", "_")


# Write per-file error reports
for file_path, issues in by_file.items():
    output_path = os.path.join(ERRORS, safe_name(file_path) + ".err.txt")
    with open(output_path, "w", encoding="utf-8") as out:
        for issue in issues:
            if issue["line"] is None:
                location = issue["file"]
            elif issue["col"] is None:
                location = f"{issue['file']}({issue['line']})"
            else:
                location = f"{issue['file']}({issue['line']},{issue['col']})"

            code_suffix = f" {issue['code']}" if issue["code"] else ""
            out.write(f"{location}: {issue['kind']}{code_suffix}: {issue['message']}\n")


index = {
    "total_issues": len(all_issues),
    "files_with_issues": len(by_file),
    "by_file": by_file,
}
with open(os.path.join(ERRORS, "index.json"), "w", encoding="utf-8") as out:
    json.dump(index, out, ensure_ascii=False, indent=2)

print(f"Parsed {len(all_issues)} issues across {len(by_file)} files.")
