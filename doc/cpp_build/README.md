# C++Builder (RAD Studio) Build Automation for Windows

This pack helps you:
1) Build `.cbproj` with MSBuild (via `rsvars.bat`).
2) Produce rich logs (text + binary `.binlog`).
3) Parse errors into per-file reports the agent can read easily.

## Files
- `build.ps1` — runs MSBuild with logs (`logs/msbuild.log`, `logs/msbuild.errors.log`, `logs/msbuild.binlog`).
- `parse_msbuild_errors.py` — parses logs, writes `errors/*.err.txt` per source file and `errors/index.json`.
- `build_and_parse.sh` — Git Bash wrapper that runs the build then parsing.

## Usage (Git Bash)
```bash
chmod +x ./build_and_parse.sh
./build_and_parse.sh /path/to/MyApp.cbproj
# With options:
CONFIG=Debug PLATFORM=Win32 OUTDIR_WIN='C:\work\MyApp\build\' ./build_and_parse.sh ./MyApp.cbproj
```

## Usage (PowerShell)
```powershell
.\build.ps1 -ProjectPath .\MyApp.cbproj -Config Release -Platform Win64 -OutDir 'C:\work\MyApp\build\'
python .\parse_msbuild_errors.py
```

## Notes
- Adjust `RadStudioVersion` or set `-RsvarsPath` if your `rsvars.bat` lives elsewhere.
- The parser expects typical MSBuild-style error lines like `File.cpp(12,3): error E2451: ...`.
- Binary log `logs/msbuild.binlog` can be inspected with MSBuild Structured Log Viewer.
