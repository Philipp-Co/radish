#!/bin/sh
#
# Ein Paar zucchini_server + radish_server als eine Einheit fuer supervisord
# (siehe supervisord.conf, Programm "game-instance-1"): stuerzt einer der
# beiden ab oder wird das Paar per SIGTERM beendet, gehen beide zusammen --
# nie bleibt der eine ohne den anderen zurueck. supervisord sieht davon nur
# dieses Skript als einen Prozess und startet bei dessen Ende beide neu
# (autorestart=true).
#
# Erstes Argument: der Zucchini-Instanzname, an radish_server durchgereicht.
# zucchini_server selbst nimmt aktuell keine Argumente entgegen und laeuft
# immer als "zucchini" auf Port 9999 (siehe supervisord.conf) -- das Argument
# hier wirkt sich deshalb noch nicht auf zucchini_server aus, nur auf die
# Ringpuffer-/FIFO-Namen, die radish_server erwartet. Ohne Argument default
# "zucchini", passend zu zucchini_servers eigenem Default.
#
# Legt das Verzeichnis an, ueber das Django (radish/game-server/instances/
# views.py) und radish_server die Spielstart-Datei austauschen, und raeumt es
# vorher aus: nach einem Absturz soll weder ein altes Spiel wieder auftauchen
# noch eine PID liegen, hinter der kein Prozess mehr steht. Die PID-Datei
# schreibt radish_server selbst, sobald sein Handler fuer SIGUSR1 steht
# (siehe radish/game-server-core/src/main.c).
#
# Meldet sich ausserdem beim Backend an (register_instance.py, siehe dort --
# RADISH_GAME_SERVER_NAME ist die Kennung dafuer, unabhaengig vom
# Zucchini-Instanznamen oben) und wieder ab, wenn das Paar endet -- egal ob
# durch Absturz oder Signal.
set -e

ZUC_INSTANCE_NAME="${1:-zucchini}"

# Vor dem Anmelden: erst wenn die Instanz beim Backend steht, kann ein Spiel
# auf ihr starten, und dann soll das Verzeichnis schon leer bereitstehen.
# Derselbe Default wie RADISH_GAME_DATA_DIR in radish/game-server/config/
# settings.py.
GAME_DATA_DIR="${RADISH_GAME_DATA_DIR:-/run/radish}/${RADISH_GAME_SERVER_NAME}"
mkdir -p "$GAME_DATA_DIR"
rm -f "$GAME_DATA_DIR/spielstart.json" "$GAME_DATA_DIR/spielstart.json.tmp" "$GAME_DATA_DIR/zugang.json" "$GAME_DATA_DIR/radish_server.pid"
export RADISH_GAME_START_PATH="$GAME_DATA_DIR/spielstart.json"
export RADISH_GAME_PID_PATH="$GAME_DATA_DIR/radish_server.pid"

/usr/local/bin/register_instance.py register

# Die Whitelist bleibt hier leer. Zucchini nimmt nur Pakete an, deren Code
# darin steht, und die Codes vergibt das Backend je Spieler beim Spielstart
# (radish/backend/api/access.py); eingetragen werden sie erst dann, vom
# Django dieses Containers (radish/game-server/instances/views.py). Bis
# dahin kommt nichts durch -- es gibt ja auch noch kein Spiel.

/usr/local/bin/zucchini_server &
ZUCCHINI_PID=$!

# Der Admin-Client legt seine Queue selbst an, weckt den Server aber per
# Signal -- der muss dafuer schon laufen.
sleep 1

/usr/local/bin/zucchini_admin_client status

/usr/local/bin/radish_server "$ZUC_INSTANCE_NAME" &
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
