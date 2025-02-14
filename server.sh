#!/bin/bash

# --- Параметры ---
PROJECT_DIR="/root/servers/mmosource"  # Замените на путь к вашему проекту
BUILD_DIR="$PROJECT_DIR/build"
SOURCE_EXECUTABLE="/root/servers/mmorpg/mmoteeworlds_srv"  # Замените на путь к установленному исполняемому файлу
EXECUTABLE="$BUILD_DIR/mmoteeworlds_srv"  # Исполняемый файл, который создается после сборки

# --- Функция для отображения прогресса ---
show_progress() {
    local DURATION=$1
    local PROGRESS=0
    echo -n "["
    while [ $PROGRESS -lt $DURATION ]; do
        echo -n "="
        sleep 0.1
        ((PROGRESS++))
    done
    echo "]"
}

# --- Функция для обновления и сборки ---
update_and_build() {
    echo -e "\n══════════════════════════════════════════════"
    echo -e "           📦 ОБНОВЛЕНИЕ И СБОРКА ПРОЕКТА       "
    echo -e "══════════════════════════════════════════════"

    if [[ ! -d "$PROJECT_DIR" ]]; then
        echo "❌ Ошибка: Каталог проекта $PROJECT_DIR не найден."
        return
    fi

    cd "$PROJECT_DIR" || { echo "❌ Ошибка: Не удалось перейти в каталог $PROJECT_DIR."; return; }

    echo "🔄 Обновление Git-репозитория..."
    git pull && echo "✅ Git-репозиторий успешно обновлен." || { echo "❌ Ошибка: Не удалось обновить Git-репозиторий."; return; }

    if [[ ! -d "$BUILD_DIR" ]]; then
        echo "📂 Каталог сборки не найден, создаем..."
        mkdir -p "$BUILD_DIR" || { echo "❌ Ошибка: Не удалось создать каталог сборки $BUILD_DIR."; return; }
    fi

    cd "$BUILD_DIR" || { echo "❌ Ошибка: Не удалось перейти в каталог $BUILD_DIR."; return; }

    # Удаляем старый исполняемый файл, если он существует
    if [[ -f "$EXECUTABLE" ]]; then
        echo "🗑️ Удаление старого исполняемого файла..."
        rm -f "$EXECUTABLE" || { echo "❌ Ошибка: Не удалось удалить старый исполняемый файл."; return; }
    fi

    echo "🔨 Сборка проекта..."
    show_progress 30
    cmake --build . --config Debug && echo "✅ Проект успешно собран." || { echo "❌ Ошибка: Сборка проекта завершилась неудачей."; return; }

    # Копируем новый исполняемый файл из BUILD_DIR в SOURCE_EXECUTABLE
    echo "📄 Копирование нового исполняемого файла..."
    cp -f "$EXECUTABLE" "$SOURCE_EXECUTABLE" || { echo "❌ Ошибка: Не удалось скопировать новый исполняемый файл."; return; }
    echo "✅ Новый исполняемый файл успешно скопирован в $SOURCE_EXECUTABLE."
}

# --- Функция для перемещения и запуска в отладке ---
move_and_debug() {
    echo -e "\n══════════════════════════════════════════════"
    echo -e "       🛠️ ПЕРЕМЕЩЕНИЕ И ЗАПУСК В ОТЛАДКЕ        "
    echo -e "══════════════════════════════════════════════"

    if [[ ! -f "$EXECUTABLE" ]]; then
        echo "❌ Ошибка: Исполняемый файл $EXECUTABLE не найден."
        return
    fi

    # Копируем файл из BUILD_DIR в SOURCE_EXECUTABLE
    echo "📄 Копирование исполняемого файла в $SOURCE_EXECUTABLE..."
    cp -f "$EXECUTABLE" "$SOURCE_EXECUTABLE" || { echo "❌ Ошибка: Не удалось скопировать $EXECUTABLE в $SOURCE_EXECUTABLE."; return; }
    echo "✅ Исполняемый файл успешно скопирован."

    echo "📝 Создание временного скрипта команд для GDB..."
    LOG_FILE="gdb_log_$(date +%Y-%m-%d_%H).log"
    echo "set pagination off" > /tmp/gdb_commands
    echo "run" >> /tmp/gdb_commands
    echo "bt" >> /tmp/gdb_commands

    echo "🚀 Запуск GDB в фоне..."
    gdb --args mmoteeworlds_srv < /tmp/gdb_commands &> "$LOG_FILE" &

    if [[ $? -eq 0 ]]; then
        echo "✅ GDB успешно запущен в фоне. Лог сохраняется в $LOG_FILE."
    else
        echo "❌ Ошибка: GDB не удалось запустить."
    fi

    rm -f /tmp/gdb_commands
}

# --- Функция для отображения запущенных процессов GDB ---
show_gdb_processes() {
    echo -e "\n══════════════════════════════════════════════"
    echo -e "          🧐 ЗАПУЩЕННЫЕ ПРОЦЕССЫ GDB            "
    echo -e "══════════════════════════════════════════════"
    ps aux | grep "gdb" | grep "mmoteeworlds_srv" | grep -v "grep"
}

# --- Функция для завершения процессов GDB ---
kill_gdb_processes() {
    echo -e "\n══════════════════════════════════════════════"
    echo -e "         💀 ЗАВЕРШЕНИЕ ПРОЦЕССОВ GDB           "
    echo -e "══════════════════════════════════════════════"
    ps aux | grep "gdb" | grep "mmoteeworlds_srv" | grep -v "grep" | awk '{print $2}' | xargs -r kill -9
    echo "✅ Все процессы GDB для mmoteeworlds_srv завершены."
}

# --- Главное меню ---
while true; do
    if ps aux | grep "gdb" | grep "mmoteeworlds_srv" | grep -v "grep" > /dev/null; then
        ACTION_1="💀 Убить все запущенные процессы GDB для mmoteeworlds_srv"
    else
        ACTION_1="🚀 Переместить и запустить в отладке"
    fi

    echo -e "\n══════════════════════════════════════════════"
    echo -e "                🏠 ГЛАВНОЕ МЕНЮ                 "
    echo -e "══════════════════════════════════════════════"
    echo "1. $ACTION_1"
    echo "2. 📦 Обновить и собрать"
    echo "3. 🔍 Показать запущенные процессы GDB для mmoteeworlds_srv"
    echo "0. ❌ Выход"
    read -p "Введите ваш выбор: " CHOICE

    case $CHOICE in
        1)
            if [[ "$ACTION_1" == "💀 Убить все запущенные процессы GDB для mmoteeworlds_srv" ]]; then
                kill_gdb_processes
            else
                move_and_debug
            fi
            ;;
        2)
            update_and_build
            ;;
        3)
            show_gdb_processes
            ;;
        0)
            echo "👋 До свидания!"
            break
            ;;
        *)
            echo "❌ Неверный выбор, попробуйте снова."
            ;;
    esac
done
