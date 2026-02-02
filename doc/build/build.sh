#!/usr/bin/env bash
set -euo pipefail

# Отключаем переписывание путей Git Bash (MSYS2 → Win)
export MSYS2_ARG_CONV_EXCL="*"

# Абсолютные Win‑пути к инструментам/проекту
RSVARS='C:\Program Files (x86)\Embarcadero\Studio\23.0\bin\rsvars.bat'
PROJ_POSIX="${1:-$(pwd)/MyApp.cbproj}"
PROJ_WIN="$(cygpath -w "$PROJ_POSIX")"

# Параметры MSBuild (можно переопределять через ENV)
CONFIG="${CONFIG:-Release}"
PLATFORM="${PLATFORM:-Win64}"
OUTDIR_WIN="${OUTDIR_WIN:-}"  # пример: C:\work\MyApp\build\

# Доп. параметры
EXTRA=""
if [[ -n "$OUTDIR_WIN" ]]; then
  EXTRA="$EXTRA /p:OutDir=$OUTDIR_WIN"
fi

# Запуск: окружение + UTF‑8 + сборка
cmd.exe /c "call \"$RSVARS\" && chcp 65001 >NUL && msbuild \"$PROJ_WIN\" /t:Build /m /v:m /p:Config=$CONFIG /p:Platform=$PLATFORM $EXTRA"
