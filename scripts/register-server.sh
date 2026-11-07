#!/bin/bash
#
# Registriert einen Spielserver am Backend (POST /api/servers/, siehe
# radish/backend/api/interfaces/servers/views/server_views.ServerView).
#
# Authentifizierung laeuft automatisch ueber den technischen User fuer den
# Server-Teil (Rolle "Technischer-User-Server", siehe
# docker/keycloak/realm-radish.json) -- das Access-Token dafuer holt sich
# dieses Skript selbst per get-test-token.sh (im selben Verzeichnis), keine
# manuelle Token-Beschaffung noetig.
#
# Nutzung:
#   ./scripts/register-server.sh <name> <ip_address> <port>
#
# Beispiel:
#   ./scripts/register-server.sh zucchini-1 127.0.0.1 9001
#
# Ueberschreibbar per Umgebungsvariable:
#   BACKEND_URL                            (Default: http://localhost:8000)
#   TECHNICAL_USERNAME, TECHNICAL_PASSWORD (Default: technical-server-user)
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
TECHNICAL_USERNAME="${TECHNICAL_USERNAME:-technical-server-user}"
TECHNICAL_PASSWORD="${TECHNICAL_PASSWORD:-technical-server-user}"
CURL_TIMEOUT=20

NAME="${1:-}"
IP_ADDRESS="${2:-}"
PORT="${3:-}"

if [ -z "$NAME" ] || [ -z "$IP_ADDRESS" ] || [ -z "$PORT" ]; then
  echo "Nutzung: $0 <name> <ip_address> <port>" >&2
  echo "Beispiel: $0 zucchini-1 127.0.0.1 9001" >&2
  exit 1
fi

if [ ! -x "$GET_TOKEN_SCRIPT" ]; then
  echo "Fehler: $GET_TOKEN_SCRIPT nicht gefunden oder nicht ausfuehrbar." >&2
  exit 1
fi

echo "==> 1) Token fuer den technischen User ($TECHNICAL_USERNAME) holen" >&2
ACCESS_TOKEN=$("$GET_TOKEN_SCRIPT" "$TECHNICAL_USERNAME" "$TECHNICAL_PASSWORD")
TOKEN_STATUS=$?
if [ "$TOKEN_STATUS" -ne 0 ] || [ -z "$ACCESS_TOKEN" ]; then
  echo "Fehler: Konnte kein Token fuer $TECHNICAL_USERNAME holen (siehe Ausgabe von get-test-token.sh oben)." >&2
  exit 1
fi

echo "==> 2) JSON-Payload zusammenbauen" >&2
PAYLOAD=$(python3 -c "
import json, sys
try:
    port = int(sys.argv[3])
except ValueError:
    print('Fehler: Port muss eine Zahl sein.', file=sys.stderr)
    sys.exit(1)
print(json.dumps({'name': sys.argv[1], 'ip_address': sys.argv[2], 'port': port}))
" "$NAME" "$IP_ADDRESS" "$PORT")
PAYLOAD_STATUS=$?
if [ "$PAYLOAD_STATUS" -ne 0 ]; then
  exit 1
fi

echo "==> 3) Server registrieren: $NAME ($IP_ADDRESS:$PORT)" >&2
RESPONSE=$(curl -sS --max-time "$CURL_TIMEOUT" -X POST "$BACKEND_URL/api/servers/" \
  -H "Authorization: Bearer $ACCESS_TOKEN" \
  -H "Content-Type: application/json" \
  -d "$PAYLOAD" \
  -w '\n%{http_code}')
CURL_STATUS=$?
if [ "$CURL_STATUS" -ne 0 ]; then
  echo "Fehler: curl konnte die Registrierung nicht absenden (Exit-Code $CURL_STATUS). Laeuft das Backend unter $BACKEND_URL?" >&2
  exit 1
fi

HTTP_STATUS=$(printf '%s' "$RESPONSE" | tail -1)
BODY=$(printf '%s' "$RESPONSE" | sed '$d')

echo "--- Antwort (HTTP $HTTP_STATUS) ---"
if [ -n "$BODY" ]; then
  printf '%s' "$BODY" | python3 -m json.tool 2>/dev/null || printf '%s\n' "$BODY"
fi

if [ "$HTTP_STATUS" != "201" ]; then
  echo "Fehler: Server wurde nicht registriert (HTTP $HTTP_STATUS)." >&2
  exit 1
fi

echo "==> Server '$NAME' erfolgreich registriert." >&2
