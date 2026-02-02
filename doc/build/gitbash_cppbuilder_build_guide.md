# Git Bash → C++Builder (RAD Studio) Build: Быстрый старт

Этот файл — краткая инструкция и проверенные рецепты, чтобы агент стабильно запускал сборку C++Builder из **Git Bash** на Windows.

## Ключевые идеи

1) **Инициализируйте окружение через `rsvars.bat`**  
   C++Builder требует переменные (`BDS`, `BDSBIN`, `PATH`, `LIB`, `INCLUDE`). Их выставляет `rsvars.bat`. Из Git Bash `.bat` нельзя `source`, поэтому запускаем через `cmd.exe`.

2) **Отключите автоконвертацию путей Git Bash**  
   Git Bash (MSYS2) переписывает аргументы вида `C:\...`. Отключаем перед запуском инструментов:
   ```bash
   export MSYS2_ARG_CONV_EXCL="*"
   ```
   (или точечно: `MSYS2_ARG_CONV_EXCL="/p:OutDir=;/p:SomePath="`)

3) **Всегда используйте Windows‑пути и кавычки**  
   Пути с пробелами (`C:\Program Files\...`) — только в кавычках `"..."`.  
   Нужен Win‑путь к текущей папке? `cygpath -w "$PWD"`.

4) **Запускайте всё одной «сессией» `cmd.exe`**  
   Чтобы переменные из `rsvars.bat` действовали, объединяйте в одну команду:  
   `cmd.exe /c "call rsvars.bat && <build>"`

5) **Кодировка логов**  
   Если видите «кракозябры», добавьте `chcp 65001` в цепочку `cmd` (UTF‑8).

6) **Проверяйте код возврата**  
   Агент должен завершаться с ошибкой, если сборка упала: в Bash используйте `set -euo pipefail` и проверяйте `$?`.

7) **Выбирайте платформу/конфигурацию**  
   Для `.cbproj` — `msbuild /p:Config=Release /p:Platform=Win32|Win64`.  
   Для компилятора — `bcc32c`/`bcc64` с нужными `-I`/`-L`.

---

## Готовые рецепты

### A) MSBuild для `.cbproj`
```bash
export MSYS2_ARG_CONV_EXCL="*"

cmd.exe /c 'call "C:\Program Files (x86)\Embarcadero\Studio\23.0\bin\rsvars.bat" ^
 && chcp 65001 >NUL ^
 && msbuild "C:\work\MyApp\MyApp.cbproj" /t:Build /m /v:m /p:Config=Release /p:Platform=Win64'
```
> Замените `23.0` на вашу версию RAD Studio. При необходимости добавьте  
> `/p:OutDir=C:\work\MyApp\build\` (и не забудьте про `MSYS2_ARG_CONV_EXCL`).

### B) Компилятор напрямую (`bcc64`)
```bash
export MSYS2_ARG_CONV_EXCL="*"

cmd.exe /c 'call "C:\Program Files (x86)\Embarcadero\Studio\23.0\bin\rsvars.bat" ^
 && chcp 65001 >NUL ^
 && bcc64 -I"C:\3rdparty\include" -L"C:\3rdparty\lib64" -e "C:\work\MyApp\build\app.exe" "C:\work\MyApp\src\main.cpp"'
```

---

## Скрипт для агента: `build.sh`

Сохраните следующий файл рядом с проектом и вызывайте его из агента. Он:
- отключает конвертацию путей,
- конвертирует POSIX‑путь к проекту в Windows‑путь,
- инициализирует окружение через `rsvars.bat`,
- включает UTF‑8,
- запускает `msbuild`.

```bash
#!/usr/bin/env bash
set -euo pipefail

# 1) Отключаем переписывание путей Git Bash
export MSYS2_ARG_CONV_EXCL="*"

# 2) Абсолютные Win‑пути к инструментам/проекту
RSVARS='C:\Program Files (x86)\Embarcadero\Studio\23.0\bin\rsvars.bat'
PROJ_POSIX="${1:-$(pwd)/MyApp.cbproj}"
PROJ_WIN="$(cygpath -w "$PROJ_POSIX")"

# 3) Параметры MSBuild (настраиваемые)
CONFIG="${CONFIG:-Release}"
PLATFORM="${PLATFORM:-Win64}"
OUTDIR_WIN="${OUTDIR_WIN:-}"  # пример: C:\work\MyApp\build\

# Собираем доп. параметры
EXTRA=""
if [[ -n "$OUTDIR_WIN" ]]; then
  EXTRA="$EXTRA /p:OutDir=$OUTDIR_WIN"
fi

# 4) Запуск в одной сессии cmd: окружение + кодировка + сборка
cmd.exe /c "call \"$RSVARS\" && chcp 65001 >NUL && msbuild \"$PROJ_WIN\" /t:Build /m /v:m /p:Config=$CONFIG /p:Platform=$PLATFORM $EXTRA"
```

### Запуск
```bash
# Сделать исполняемым
chmod +x ./build.sh

# Простой вызов (по умолчанию Release/Win64)
./build.sh path/to/MyApp.cbproj

# С конфигурацией и каталогом вывода
CONFIG=Debug PLATFORM=Win32 OUTDIR_WIN='C:\work\MyApp\build\' ./build.sh ./MyApp.cbproj
```

---

## Частые ошибки и решения

- **`msbuild` не найден** — неверный путь к `rsvars.bat` или файл не вызван (`call ...`).  
- **Сломанные пути в `/p:OutDir=`** — забыли `MSYS2_ARG_CONV_EXCL`; Git Bash переписал `C:\...`.  
- **Пробелы в путях** — нет кавычек.  
- **32/64‑бит не совпадает** — проверьте `/p:Platform=Win32|Win64`.  
- **Русские символы в логах** — добавьте `chcp 65001`.  
- **Скрипт не падает при ошибке сборки** — используйте `set -euo pipefail` и анализируйте код возврата.

---

## Примечания по версиям RAD Studio
- Путь к `rsvars.bat` меняется с версией: `C:\Program Files (x86)\Embarcadero\Studio\<XX.Y>\bin\rsvars.bat`.  
- Примеры используют `23.0` как заглушку — замените на вашу.

Удачных сборок!
