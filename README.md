# Daily Diary Widget

Ежедневник для Windows с красивым виджетом в системном трее.

## Возможности

- Всплывающий виджет — полупрозрачный, со скруглёнными углами
- Иконка в системном трее
- Горячая клавиша Ctrl+Alt+D
- Приоритеты записей (низкий / средний / высокий)
- Автосохранение в файл diary.dat
- Автозапуск с Windows

## Скачать

Готовый diary_widget.exe — во вкладке Releases.

## Использование

1. Скачай diary_widget.exe из Releases
2. Запусти — программа появится в трее
3. ЛКМ по иконке — показать виджет
4. ПКМ по иконке — меню (редактор, автозапуск, выход)
5. Ctrl+Alt+D — открыть/скрыть редактор

## Сборка из исходников

Через MSYS2 UCRT64:

    g++ diary_widget.cpp -o diary_widget.exe -mwindows -municode -lcomctl32 -ldwmapi -lgdiplus -luser32 -lgdi32 -lole32 -lshell32 -ladvapi32 -static -s -O2

## Лицензия

MIT
