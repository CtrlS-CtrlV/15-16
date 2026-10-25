#!/bin/bash
# test_servers.sh — тестування 3 архітектур серверів
# Використання: ./test_servers.sh <server_binary> <num_clients>

SERVER=$1
NUM_CLIENTS=${2:-10}
PORT=9000
DURATION=3

if [ -z "$SERVER" ]; then
    echo "Використання: $0 <server_binary> <num_clients>"
    exit 1
fi

echo "=== Тестування: $SERVER з $NUM_CLIENTS клієнтами ==="

# Запуск сервера у фоновому режимі
./$SERVER &
SERVER_PID=$!
sleep 1

# Запуск клієнтів паралельно через netcat
for ((i=1; i<=NUM_CLIENTS; i++)); do
    (
        for ((j=1; j<=DURATION; j++)); do
            echo "Msg $j from client $i" | nc 127.0.0.1 $PORT
            sleep 1
        done
    ) &
done

# Очікування завершення клієнтів
sleep $((DURATION + 2))

# Збір інформації про використання пам'яті RSS
if [ -f /proc/$SERVER_PID/status ]; then
    echo "--- Статистика пам'яті сервера (PID $SERVER_PID) ---"
    grep "VmRSS" /proc/$SERVER_PID/status
else
    echo "Сервер завершився або недоступний."
fi

# Завершення сервера
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
echo "=== Тест завершено ==="
