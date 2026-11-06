#!/bin/bash
#
# Erstellt ein Spiel am Backend (POST /api/client/game/, siehe
# radish/backend/api/client_views.GameCreateView).
#
# Authentifizierung laeuft automatisch ueber einen normalen Spieler (Rolle
# "Player", siehe docker/keycloak/realm-radish.json) -- das Access-Token
# dafuer holt sich dieses Skript selbst per get-test-token.sh (im selben
# Verzeichnis), keine manuelle Token-Beschaffung noetig. Wer der Ersteller
# ist, ergibt sich aus diesem Token (request.user im Backend) -- ein
# player_identifier wird nicht mehr uebergeben, siehe GameCreateSerializer.
#
# Nutzung:
#   ./scripts/create-game.sh <name> <password>
#
# Beispiel:
#   ./scripts/create-game.sh mein-spiel geheim123
#
# Ueberschreibbar per Umgebungsvariable:
#   BACKEND_URL                      (Default: http://localhost:8000)
#   PLAYER_USERNAME, PLAYER_PASSWORD (Default: test-user)
#   Alles, was get-test-token.sh selbst schon ueberschreibbar macht
#   (KEYCLOAK_BASE_URL, REALM, CLIENT_ID, REDIRECT_URI).

set -uo pipefail
# Bewusst OHNE "-e", aus demselben Grund wie in get-test-token.sh: eine
# fehlschlagende Pipeline in einer VAR=$(...)-Zuweisung wuerde das Skript
# sonst vor der eigenen Fehlerbehandlung beenden. Stattdessen wird nach
# jedem kritischen Schritt explizit der Exit-Code geprueft.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GET_TOKEN_SCRIPT="$SCRIPT_DIR/get-test-token.sh"

BACKEND_URL="${BACKEND_URL:-http://localhost:8000}"
PLAYER_USERNAME="${PLAYER_USERNAME:-test-user}"
PLAYER_PASSWORD="${PLAYER_PASSWORD:-test-user}"
CURL_TIMEOUT=20

NAME="${1:-}"
PASSWORD="${2:-}"

if [ -z "$NAME" ] || [ -z "$PASSWORD" ]; then
  echo "Nutzung: $0 <name> <password>" >&2
  echo "Beispiel: $0 mein-spiel geheim123" >&2
  exit 1
fi

if [ ! -x "$GET_TOKEN_SCRIPT" ]; then
  echo "Fehler: $GET_TOKEN_SCRIPT nicht gefunden oder nicht ausfuehrbar." >&2
  exit 1
fi

echo "==> 1) Token fuer den Spieler ($PLAYER_USERNAME) holen" >&2
ACCESS_TOKEN=$("$GET_TOKEN_SCRIPT" "$PLAYER_USERNAME" "$PLAYER_PASSWORD")
TOKEN_STATUS=$?
if [ "$TOKEN_STATUS" -ne 0 ] || [ -z "$ACCESS_TOKEN" ]; then
  echo "Fehler: Konnte kein Token fuer $PLAYER_USERNAME holen (siehe Ausgabe von get-test-token.sh oben)." >&2
  exit 1
fi

echo "==> 2) JSON-Payload zusammenbauen" >&2
PAYLOAD=$(python3 -c "
import json, sys
print(json.dumps({'name': sys.argv[1], 'password': sys.argv[2]}))
" "$NAME" "$PASSWORD")

echo "==> 3) Spiel erstellen: $NAME" >&2
RESPONSE=$(curl -sS --max-time "$CURL_TIMEOUT" -X POST "$BACKEND_URL/api/client/game/" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d "$PAYLOAD" \
  -w '\n%{http_code}')
CURL_STATUS=$?
if [ "$CURL_STATUS" -ne 0 ]; then
  echo "Fehler: curl konnte die Anfrage nicht absenden (Exit-Code $CURL_STATUS). Laeuft das Backend unter $BACKEND_URL?" >&2
  exit 1
fi

HTTP_STATUS=$(printf '%s' "$RESPONSE" | tail -1)
BODY=$(printf '%s' "$RESPONSE" | sed '$d')

echo "--- Antwort (HTTP $HTTP_STATUS) ---"
if [ -n "$BODY" ]; then
  printf '%s' "$BODY" | python3 -m json.tool 2>/dev/null || printf '%s\n' "$BODY"
fi

if [ "$HTTP_STATUS" != "201" ]; then
  echo "Fehler: Spiel wurde nicht erstellt (HTTP $HTTP_STATUS)." >&2
  exit 1
fi

echo "==> Spiel '$NAME' erfolgreich erstellt." >&2
