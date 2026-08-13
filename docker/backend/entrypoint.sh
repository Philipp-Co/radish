#!/bin/sh
#
# Zwei Prozesse, einer davon im Vordergrund -- aber keiner von beiden als PID 1:
# Docker schickt SIGTERM nur an PID 1, und das ist diese Shell. Beide Programme
# laufen deshalb im Hintergrund, die Shell wartet auf den Spielserver und gibt
# das Signal an beide weiter. Liefe der Spielserver stattdessen im Vordergrund,
# kaeme der Trap erst dran, wenn er von sich aus endet -- also nie, und Docker
# muesste den Container nach der Frist hart abschiessen.
#
set -e

ZUC_INSTANCE_NAME="zucchini"

# Der Code, den der Client jedem Paket voranstellt (ZUC_CODE in
# radish/client/src/main.c). Zucchini nimmt nur Pakete an, deren Code in der
# Whitelist steht, und merkt sich dabei die Absenderadresse -- ohne diesen
# Eintrag verwirft es alles und es kaeme auch keine Antwort zurueck. Das Argument
# ist hexadezimal.
ZUC_CLIENT_CODE="1"

/usr/local/bin/zucchini_server &
ZUCCHINI_PID=$!

# Der Admin-Client legt seine Queue selbst an, weckt den Server aber per Signal
# -- der muss dafuer schon laufen.
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

# "|| EXIT=$?" statt bloss "wait": mit set -e wuerde ein Rueckgabewert ungleich
# 0 -- auch der aus einem Signal -- das Skript sofort verlassen, und zucchini
# bliebe ungetoetet zurueck.
EXIT=0
wait "$RADISH_PID" || EXIT=$?

kill -TERM "$ZUCCHINI_PID" 2>/dev/null || true
wait "$ZUCCHINI_PID" 2>/dev/null || true

exit "$EXIT"
