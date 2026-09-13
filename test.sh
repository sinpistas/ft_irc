#!/bin/bash

# Configuración del servidor
HOST="127.0.0.1"
PORT=2020
PASS="samurai"

# Función para simular un cliente y enviar comandos con retardos (sleep)
run_client_1() {
  sleep 0.5; echo "PASS $PASS"
  sleep 0.5; echo "NICK alice"
  sleep 0.5; echo "USER alice 0 * :Alice Smith"
  sleep 0.5; echo "JOIN #42malaga"
  sleep 0.5; echo "PRIVMSG #42malaga :Hola a todos desde el script"
  sleep 0.5; echo "JOIN 0"
  sleep 0.5; echo "QUIT :Adios"
}

echo "=== Conectando Cliente 1 ==="
# Inyectamos la función a netcat. Las líneas enviadas deben llevar finales CRLF (\r\n)
run_client_1 | nc -C $HOST $PORT