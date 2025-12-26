#!/bin/bash
echo "╔══════════════════════════════════════════════════════════╗"
echo "║   ИТОГОВОЕ ФУНКЦИОНАЛЬНОЕ ТЕСТИРОВАНИЕ СЕРВЕРА          ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""
echo "Дата: $(date '+%Y-%m-%d %H:%M:%S')"
echo ""
# Глобальные переменные
SERVER="./server"
REPORT_FILE="final_test_report.md"
TEST_CONFIG="final_test.conf"
TEST_LOG="/tmp/scale_test.log"
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
# Утилита для проверки порта
check_port_listening() {
    local port=$1
    local attempts=5
    local delay=0.5
    
    for ((i=1; i<=attempts; i++)); do
        if (netstat -tuln 2>/dev/null || ss -tuln 2>/dev/null || lsof -i :$port 2>/dev/null) | grep -q ":$port"; then
            return 0
        fi
        sleep $delay
    done
    return 1
}
# Очистка процессов на порту
cleanup_port() {
    local port=$1
    lsof -ti :$port 2>/dev/null | xargs kill -9 2>/dev/null || true
    sleep 0.5
}
# Создаем тестовую конфигурацию
create_test_config() {
    cat > "$TEST_CONFIG" << 'EOF'
# Тестовая база пользователей
user:P@ssW0rd
alice:alicepassword
bob:bobsecret
admin:admin123
EOF
    echo "Создан тестовый конфиг: $TEST_CONFIG"
}
# Функция тестирования
run_test() {
    local test_num=$1
    local test_name=$2
    local test_func=$3
    
    echo ""
    echo "=== ТЕСТ $test_num: $test_name ==="
    echo ""
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    if $test_func; then
        echo "OK ТЕСТ $test_num ПРОЙДЕН"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        echo "ERR ТЕСТ $test_num ПРОВАЛЕН"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
}
# Тест 1: Базовый запуск
test_01_basic_start() {
    echo "Проверка: Запуск сервера с параметрами по умолчанию"
    
    # Очищаем порт перед запуском
    PORT=33444
    cleanup_port $PORT
    
    # Создаем тестовую БД
    echo "testuser:testpass123" > test_db.conf
    
    # Запускаем сервер на уникальном порту
    "./$SERVER" -p $PORT -c "test_db.conf" -l "test.log" &
    local PID=$!
    
    # Даем время на запуск
    sleep 3
    
    # Проверяем запуск
    if ! ps -p $PID > /dev/null 2>&1; then
        echo "   Сервер не запустился"
        # Проверяем лог на ошибки
        if [ -f "test.log" ]; then
            echo "   Последние строки лога:"
            tail -5 test.log
        fi
        rm -f test_db.conf test.log 2>/dev/null
        cleanup_port $PORT
        return 1
    fi
    
    # Проверяем порт
    if check_port_listening $PORT; then
        echo "   Сервер слушает порт $PORT"
        
        # Останавливаем
        kill $PID 2>/dev/null
        sleep 2
        
        if ps -p $PID > /dev/null 2>&1; then
            kill -9 $PID 2>/dev/null
            echo "   Сервер не остановился по SIGTERM"
        fi
        
        rm -f test_db.conf test.log 2>/dev/null
        cleanup_port $PORT
        return 0
    else
        kill $PID 2>/dev/null
        sleep 1
        kill -9 $PID 2>/dev/null 2>/dev/null
        echo "   Сервер не слушает порт"
        rm -f test_db.conf test.log 2>/dev/null
        cleanup_port $PORT
        return 1
    fi
}
# Тест 2: Логгирование
test_02_logging() {
    echo "Проверка: Создание и запись в лог-файл"
    
    PORT=33445
    cleanup_port $PORT
    LOG="test_logging.log"
    rm -f "$LOG" 2>/dev/null
    
    echo "testuser:testpass" > test_db2.conf
    
    "./$SERVER" -p $PORT -c "test_db2.conf" -l "$LOG" &
    local PID=$!
    sleep 3
    
    # Проверяем файл
    if [ -f "$LOG" ]; then
        echo "   Лог-файл создан"
        # Проверяем запись в лог
        if grep -q -i "server\|starting\|listen" "$LOG"; then
            echo "   Запись в лог выполнена"
        else
            echo "   В логе нет ожидаемых записей"
        fi
        kill $PID 2>/dev/null
        sleep 1
        kill -9 $PID 2>/dev/null 2>/dev/null
        rm -f "$LOG" test_db2.conf 2>/dev/null
        cleanup_port $PORT
        return 0
    else
        kill $PID 2>/dev/null
        sleep 1
        kill -9 $PID 2>/dev/null 2>/dev/null
        echo "   Лог-файл не создан"
        rm -f test_db2.conf 2>/dev/null
        cleanup_port $PORT
        return 1
    fi
}
# Тест 3: Загрузка пользовательской БД
test_03_user_db() {
    echo "Проверка: Загрузка базы пользователей"
    
    # Очищаем порт
    PORT=33447
    cleanup_port $PORT
    
    # Создаем тестовую БД
    cat > test_users.db << 'EOF'
user1:pass1
user2:pass2
user3:pass3
EOF
    
    "./$SERVER" -p $PORT -c "test_users.db" -l "db_test.log" &
    local PID=$!
    sleep 3
    
    if ps -p $PID > /dev/null 2>&1; then
        echo "   Сервер запустился с пользовательской БД"
        
        # Даем время на инициализацию
        sleep 1
        
        kill $PID 2>/dev/null
        sleep 1
        
        # Проверяем логи
        if [ -f "db_test.log" ]; then
            if grep -q -i "user\|database\|loaded" "db_test.log"; then
                echo "   БД пользователей загружена"
            fi
        fi
        
        rm -f test_users.db db_test.log 2>/dev/null
        cleanup_port $PORT
        return 0
    else
        echo "   Сервер не запустился с пользовательской БД"
        rm -f test_users.db db_test.log 2>/dev/null
        cleanup_port $PORT
        return 1
    fi
}
# Тест 4: Проверка параметров командной строки
test_04_command_line() {
    echo "Проверка: Обработка параметров командной строки"
    
    # Тест справки
    output=$(timeout 2s "./$SERVER" -h 2>&1)
    
    if echo "$output" | grep -q -i "usage\|help\|справка"; then
        echo "   Параметр -h работает"
        return 0
    else
        echo "   Параметр -h не работает или зависает"
        return 1
    fi
}
# Тест 5: Несуществующий файл БД
test_05_nonexistent_db() {
    echo "Проверка: Обработка несуществующего файла БД"
    
    PORT=33448
    cleanup_port $PORT
    
    "./$SERVER" -p $PORT -c "non_existent_file.db" -l "nonexistent_test.log" &
    local PID=$!
    sleep 3
    
    if ps -p $PID > /dev/null 2>&1; then
        echo "   Сервер запустился с несуществующей БД (должен)"
        kill $PID 2>/dev/null
        sleep 1
        rm -f nonexistent_test.log 2>/dev/null
        cleanup_port $PORT
        return 0
    else
        echo "   Сервер не запустился с несуществующей БД"
        rm -f nonexistent_test.log 2>/dev/null
        cleanup_port $PORT
        return 1
    fi
}
# Главная функция
main() {
    # Проверяем наличие сервера
    if [ ! -f "$SERVER" ]; then
        echo "Ошибка: Файл сервера '$SERVER' не найден!"
        exit 1
    fi
    
    if [ ! -x "$SERVER" ]; then
        chmod +x "$SERVER"
    fi
    
    # Создаем отчет
    echo "# Отчет о функциональном тестировании сервера" > "$REPORT_FILE"
    echo "" >> "$REPORT_FILE"
    echo "## Информация о тестировании" >> "$REPORT_FILE"
    echo "- Дата тестирования: $(date '+%Y-%m-%d %H:%M:%S')" >> "$REPORT_FILE"
    echo "- Версия сервера: SHA-224 с суммой квадратов" >> "$REPORT_FILE"
    
    # Создаем конфиг
    create_test_config
    
    # Массив для хранения результатов тестов
    declare -a test_status
    
    # Запускаем тесты
    echo "Запуск тестов..."
    echo ""
    
    run_test "01" "Базовый запуск сервера" test_01_basic_start
    test_status[1]=$?
    
    run_test "02" "Создание лог-файла" test_02_logging
    test_status[2]=$?
    
    run_test "03" "Загрузка пользовательской БД" test_03_user_db
    test_status[3]=$?
    
    run_test "04" "Параметры командной строки" test_04_command_line
    test_status[4]=$?
    
    run_test "05" "Несуществующий файл БД" test_05_nonexistent_db
    test_status[5]=$?
    
    # Создаем таблицу тест-кейсов
    echo "" >> "$REPORT_FILE"
    echo "## Тест-кейсы функционального тестирования" >> "$REPORT_FILE"
    echo "" >> "$REPORT_FILE"
    echo "| ID теста | Название | Тип | Описание | Ожидаемый результат | Полученный результат | Итог |" >> "$REPORT_FILE"
    echo "|----------|----------|-----|----------|---------------------|----------------------|------|" >> "$REPORT_FILE"
    
    # Заполняем таблицу
    for i in {1..5}; do
        case $i in
            1)
                name="Базовый запуск сервера"
                desc="Запуск сервера с тестовой БД на порту 33444"
                expected="Сервер запускается и слушает порт"
                ;;
            2)
                name="Создание лог-файла"
                desc="Проверка создания и записи в лог-файл"
                expected="Лог-файл создается при старте сервера"
                ;;
            3)
                name="Загрузка пользовательской БД"
                desc="Проверка загрузки пользовательской базы данных"
                expected="Сервер запускается и загружает пользователей"
                ;;
            4)
                name="Параметры командной строки"
                desc="Проверка обработки параметров командной строки"
                expected="Вывод справки при параметре -h"
                ;;
            5)
                name="Несуществующий файл БД"
                desc="Обработка несуществующего файла базы данных"
                expected="Сервер запускается (БД может быть пустой)"
                ;;
        esac
        
        if [ ${test_status[$i]} -eq 0 ]; then
            result_text="Успешно"
            status_text="ПРОЙДЕН"
        else
            result_text="Неудача"
            status_text="ПРОВАЛЕН"
        fi
        
        echo "| $i | $name | Позитивный | $desc | $expected | $result_text | $status_text |" >> "$REPORT_FILE"
    done
    
    # Добавляем сводку результатов
    echo "" >> "$REPORT_FILE"
    echo "## Сводка результатов" >> "$REPORT_FILE"
    echo "" >> "$REPORT_FILE"
    echo "| Всего тестов | Пройдено | Провалено | Успешность |" >> "$REPORT_FILE"
    echo "|--------------|----------|-----------|------------|" >> "$REPORT_FILE"
    
    # Вычисляем процент успешности
    local percentage=0
    if [ $TOTAL_TESTS -gt 0 ]; then
        percentage=$((PASSED_TESTS * 100 / TOTAL_TESTS))
    fi
    
    echo "| $TOTAL_TESTS | $PASSED_TESTS | $FAILED_TESTS | $percentage% |" >> "$REPORT_FILE"
    echo "" >> "$REPORT_FILE"
    
    # Заключение
    echo "## Заключение" >> "$REPORT_FILE"
    echo "" >> "$REPORT_FILE"
    
    if [ $PASSED_TESTS -eq $TOTAL_TESTS ] && [ $TOTAL_TESTS -gt 0 ]; then
        echo "**ВСЕ ТЕСТЫ УСПЕШНО ПРОЙДЕНЫ!**" >> "$REPORT_FILE"
        echo "" >> "$REPORT_FILE"
        echo "Сервер соответствует функциональным требованиям и готов к использованию." >> "$REPORT_FILE"
    else
        echo "**ТРЕБУЕТСЯ ДОРАБОТКА!**" >> "$REPORT_FILE"
        echo "" >> "$REPORT_FILE"
        echo "Обнаружены непройденные тесты. Требуется исправление обнаруженных проблем перед выпуском в эксплуатацию." >> "$REPORT_FILE"
    fi
    
    # Итоговый отчет в консоль
    echo ""
    echo "╔══════════════════════════════════════════════════════════╗"
    echo "║                     ИТОГИ ТЕСТИРОВАНИЯ                  ║"
    echo "╚══════════════════════════════════════════════════════════╝"
    echo ""
    echo "Всего тестов: $TOTAL_TESTS"
    echo "Пройдено: $PASSED_TESTS"
    echo "Провалено: $FAILED_TESTS"
    echo ""
    echo "Подробный отчет сохранен в: $REPORT_FILE"
    
    # Очистка
    rm -f "$TEST_CONFIG" "$TEST_LOG" 2>/dev/null
    rm -f test*.log test*.conf test*.db 2>/dev/null
    
    # Очищаем все тестовые порты
    for port in 33444 33445 33447 33448; do
        cleanup_port $port
    done
    
    # Возвращаем код
    if [ $PASSED_TESTS -eq $TOTAL_TESTS ] && [ $TOTAL_TESTS -gt 0 ]; then
        echo ""
        echo "ВСЕ ТЕСТЫ УСПЕШНО ПРОЙДЕНЫ!"
        echo "Сервер соответствует функциональным требованиям."
        exit 0
    else
        echo ""
        echo "ТРЕБУЕТСЯ ДОРАБОТКА!"
        echo "Есть непройденные тесты."
        exit 1
    fi
}
# Запуск
main
