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

### Build stability options

`build.ps1` now includes extra reliability controls for C++Builder intermediate-file races (`E74`):

- `-PreClean $true|$false` (default: `$true`)  
  Runs `msbuild /t:Clean` before the main build.
- `-PurgeIntermediates $true|$false` (default: `$true`)  
  Removes stale `*.tmp` / `*.o.tmp` and `<ProjectName>.o` in `<ProjectDir>\<Platform>\<Config>`.
- `-RetryOnE74 <N>` (default: `1`)  
  Retries build up to `N` times when `error E74` with `permission denied` is detected.
- `-RetryDelayMs <ms>` (default: `1200`)  
  Delay before retry.
- `-LockWaitMs <ms>` (default: `5000`)  
  Wait timeout for locked primary object file (`<ProjectName>.o`) before clean/build.
- `-FixIntermediateAcl $true|$false` (default: `$false`)  
  Tries to grant `Modify` ACL on `<ProjectDir>\<Platform>\<Config>` if delete probe fails (`MSB3061` / `E74` scenarios).

Example:

```powershell
.\build.ps1 -ProjectPath .\MyApp.cbproj -Config Debug -Platform Win64x -RetryOnE74 2 -RetryDelayMs 1500 -LockWaitMs 8000 -FixIntermediateAcl $true
python .\parse_msbuild_errors.py
```

## Notes
- Adjust `RadStudioVersion` or set `-RsvarsPath` if your `rsvars.bat` lives elsewhere.
- The parser handles source errors and toolchain-level entries like `CodeGear.Cpp.Targets(...): error E74: ...`.
- Binary log `logs/msbuild.binlog` can be inspected with MSBuild Structured Log Viewer.
