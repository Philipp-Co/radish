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
# Meldet sich ausserdem beim Backend an (register_instance.py, siehe dort --
# RADISH_GAME_SERVER_NAME ist die Kennung dafuer, unabhaengig vom
# Zucchini-Instanznamen oben) und wieder ab, wenn das Paar endet -- egal ob
# durch Absturz oder Signal.
set -e

ZUC_INSTANCE_NAME="${1:-zucchini}"

/usr/local/bin/register_instance.py register

# Der Code, den der Client jedem Paket voranstellt. Zucchini nimmt nur
# Pakete an, deren Code in der Whitelist steht -- ohne diesen Eintrag
# verwirft es alles. Das Argument ist hexadezimal.
ZUC_CLIENT_CODE="1"

/usr/local/bin/zucchini_server &
ZUCCHINI_PID=$!

# Der Admin-Client legt seine Queue selbst an, weckt den Server aber per
# Signal -- der muss dafuer schon laufen.
sleep 1

/usr/local/bin/zucchini_admin_client whitelist-add "$ZUC_CLIENT_CODE"
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
