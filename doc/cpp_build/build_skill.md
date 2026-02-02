# Скилл `/build` для Claude Code

## Назначение

Slash-команда `/build` позволяет Claude Code собирать проект dbMCP из C++Builder, автоматически парсить ошибки компиляции и предлагать исправления.

## Расположение

```
.claude/commands/build.md    — определение скилла (slash-команда)
doc/cpp_build/build.ps1      — PowerShell-скрипт сборки через MSBuild
doc/cpp_build/parse_msbuild_errors.py — парсер ошибок MSBuild в JSON
doc/cpp_build/logs/          — лог-файлы MSBuild (создаются при сборке)
doc/cpp_build/errors/        — результаты парсинга: index.json + *.err.txt
```

## Использование

В Claude Code:

```
/build          — собрать в конфигурации Debug (по умолчанию)
/build Release  — собрать в конфигурации Release
```

Claude Code также автоматически использует этот скилл при отладке: если вносятся изменения в код, он может запустить сборку, прочитать ошибки и предложить исправления.

## Как это работает

1. **Очистка** — удаление старых логов и отчётов об ошибках.
2. **Сборка** — запуск `build.ps1`, который вызывает MSBuild через `cmd.exe` с настроенным окружением RAD Studio (`rsvars.bat`).
3. **Парсинг** — `parse_msbuild_errors.py` разбирает `msbuild.errors.log`, создаёт:
   - `errors/index.json` — JSON-индекс со счётчиком ошибок и данными по файлам.
   - `errors/<file>.err.txt` — текстовые отчёты по каждому файлу с ошибками.
4. **Отчёт** — Claude Code читает `index.json`, при наличии ошибок читает исходники и предлагает исправления.

## Формат index.json

```json
{
  "total_issues": 1,
  "files_with_issues": 1,
  "by_file": {
    "uMain.cpp": [
      {
        "file": "uMain.cpp",
        "line": 15,
        "col": 13,
        "kind": "error",
        "code": "E4656",
        "message": "use of undeclared identifier 'UNDEFINED_SYMBOL'"
      }
    ]
  }
}
```

## Технические детали

- MSBuild 4.x (из RAD Studio) не поддерживает `/bl` (binary log).
- Аргументы MSBuild с `;` (например, `/flp:...;verbosity=diagnostic`) обёрнуты в кавычки для корректной передачи через `cmd.exe`.
- Парсер обрабатывает MSBuild node-prefix `1>` и trailing `[project.cbproj]` в строках ошибок.
- Platform по умолчанию `Win64x`, конфигурация `Debug`.
