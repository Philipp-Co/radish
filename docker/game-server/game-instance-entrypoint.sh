#!/bin/sh
#
# Ein Paar zucchini_server + radish_server als eine Einheit fuer supervisord
# (siehe supervisord.conf, Programme "game-instance-1" bis "-4"): stuerzt einer der
# beiden ab oder wird das Paar per SIGTERM beendet, gehen beide zusammen --
# nie bleibt der eine ohne den anderen zurueck. supervisord sieht davon nur
# dieses Skript als einen Prozess und startet bei dessen Ende beide neu
# (autorestart=true).
#
# Aufruf: game-instance-entrypoint.sh <name> <port> (siehe supervisord.conf,
# je Instanz ein [program:...]-Abschnitt). Das Paar wird nur dort festgelegt
# und von hier aus durchgereicht:
#
#   <name>  Name der Instanz in diesem Container -- Zucchini-Name
#           (zucchini_server/zucchini_admin_client -n), Name fuer
#           radish_server (Ringpuffer/FIFO), Datenverzeichnis und Pfadteil
#           der Instanz-API (radish/game-server/instances/views.py). Muss nur
#           in diesem Container eindeutig sein.
#   <port>  UDP-Port von zucchini_server (-p), so auch beim Backend gemeldet.
#           Ebenfalls je Container eindeutig, und in docker-compose.yaml
#           nach aussen gemappt.
#
# Kein Default fuer beides: zucchini_server verlangt beides auch, und ein
# stiller Default wuerde mit der naechsten Instanz zusammenstossen.
#
# Legt das Verzeichnis an, ueber das Django (radish/game-server/instances/
# views.py) und radish_server die Spielstart-Datei austauschen, und raeumt es
# vorher aus: nach einem Absturz soll weder ein altes Spiel wieder auftauchen
# noch eine PID liegen, hinter der kein Prozess mehr steht. Die PID-Datei
# schreibt radish_server selbst, sobald sein Handler fuer SIGUSR1 steht
# (siehe radish/game-server-core/src/main.c).
#
# Meldet sich ausserdem beim Backend an (register_instance.py) und wieder ab,
# wenn das Paar endet -- egal ob durch Absturz oder Signal. Das Backend
# braucht dafuer einen Namen, der ueber alle Container eindeutig ist
# (GameServer.name): "<RADISH_GAME_SERVER_ID>-<name>", mit der Kennung des
# Containers aus docker-compose.yaml.
set -e

if [ $# -ne 2 ] || [ -z "$1" ] || [ -z "$2" ]; then
    echo "Aufruf: $0 <name> <port>" >&2
    exit 1
fi
if [ -z "$RADISH_GAME_SERVER_ID" ]; then
    echo "RADISH_GAME_SERVER_ID ist nicht gesetzt." >&2
    exit 1
fi

# Fuer register_instance.py -- auch fuer dessen Aufruf durch radish_server
# beim Spielende (RADISH_GAME_FINISHED_COMMAND unten), der die Umgebung von
# hier erbt.
export RADISH_GAME_INSTANCE_NAME="$1"
export RADISH_GAME_SERVER_PORT="$2"
export RADISH_GAME_SERVER_NAME="${RADISH_GAME_SERVER_ID}-${RADISH_GAME_INSTANCE_NAME}"

# Vor dem Anmelden: erst wenn die Instanz beim Backend steht, kann ein Spiel
# auf ihr starten, und dann soll das Verzeichnis schon leer bereitstehen.
# Derselbe Default wie RADISH_GAME_DATA_DIR in radish/game-server/config/
# settings.py. Unter dem Namen im Container, nicht dem beim Backend: Django
# findet das Verzeichnis ueber den Namen in der URL, und das ist dieser.
GAME_DATA_DIR="${RADISH_GAME_DATA_DIR:-/run/radish}/${RADISH_GAME_INSTANCE_NAME}"
mkdir -p "$GAME_DATA_DIR"
rm -f "$GAME_DATA_DIR/spielstart.json" "$GAME_DATA_DIR/spielstart.json.tmp" "$GAME_DATA_DIR/zugang.json" "$GAME_DATA_DIR/radish_server.pid"
export RADISH_GAME_START_PATH="$GAME_DATA_DIR/spielstart.json"
export RADISH_GAME_PID_PATH="$GAME_DATA_DIR/radish_server.pid"
# Mit diesem Programm meldet radish_server das Ende eines Spiels an das Backend
# (register_instance.py game-finished, siehe dort und
# radish/game-server-core/src/interface/game_report.h).
export RADISH_GAME_FINISHED_COMMAND=/usr/local/bin/register_instance.py

/usr/local/bin/register_instance.py register

# Die Whitelist bleibt hier leer. Zucchini nimmt nur Pakete an, deren Code
# darin steht, und die Codes vergibt das Backend je Spieler beim Spielstart
# (radish/backend/api/access.py); eingetragen werden sie erst dann, vom
# Django dieses Containers (radish/game-server/instances/views.py). Bis
# dahin kommt nichts durch -- es gibt ja auch noch kein Spiel.

/usr/local/bin/zucchini_server -n "$RADISH_GAME_INSTANCE_NAME" -p "$RADISH_GAME_SERVER_PORT" &
ZUCCHINI_PID=$!

# Der Admin-Client legt seine Queue selbst an, weckt den Server aber per
# Signal -- der muss dafuer schon laufen.
sleep 1

/usr/local/bin/zucchini_admin_client -n "$RADISH_GAME_INSTANCE_NAME" status

/usr/local/bin/radish_server "$RADISH_GAME_INSTANCE_NAME" &
RADISH_PID=$!

terminate()
{
    kill -TERM "$RADISH_PID" "$ZUCCHINI_PID" 2>/dev/null || true
}
trap terminate TERM INT

# "|| EXIT=$?" statt bloss "wait": mit set -e wuerde ein Rueckgabewert
# ungleich 0 -- auch der aus einem Signal -- das Skript sofort verlassen,
# und zucchini bliebe ungetoetet zurueck.
EXIT=0
wait "$RADISH_PID" || EXIT=$?

kill -TERM "$ZUCCHINI_PID" 2>/dev/null || true
wait "$ZUCCHINI_PID" 2>/dev/null || true

# Nicht fatal (siehe register_instance.py, deregister()): das Backend zu
# erreichen ist beim Herunterfahren kein Grund, das Beenden dieser Instanz
# zu blockieren.
/usr/local/bin/register_instance.py deregister || true

exit "$EXIT"
