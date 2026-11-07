#!/bin/bash
#
# Prueft die Komplexitaet jeder Funktion im Backend mit lizard. lizard hat
# keine Konfigurationsdatei -- die verbindlichen Grenzwerte stehen deshalb
# hier und nur hier:
#
#   zyklomatische Komplexitaet (CCN)  hoechstens 5
#   Laenge einer Funktion             hoechstens 500 Zeilen
#   Parameter                         hoechstens 6
#
# Keine Erweiterungen (-E...). Migrationen sind generiert und ausgenommen.
#
# Ausnahmen fuer einzelne Funktionen: "# #lizard forgives" in der Funktion,
# mit einem Kommentar, warum.
#
# Nutzung (lizard aus requirements-dev.txt muss installiert sein):
#   radish/backend/scripts/lizard.sh            # api, config, web
#   radish/backend/scripts/lizard.sh api/models # nur bestimmte Pfade
#
# Gibt nur die Warnungen aus; Exit-Code ungleich 0, sobald eine Grenze
# ueberschritten ist.

set -euo pipefail

# Aus jedem Verzeichnis aufrufbar: Pfade gelten relativ zu radish/backend/.
cd "$(dirname "$0")/.."

if [ "$#" -eq 0 ]; then
    set -- api config web
fi

exec lizard \
    --languages python \
    --exclude "*/migrations/*" \
    --warnings_only \
    --CCN 5 \
    --length 500 \
    --arguments 6 \
    "$@"
