#!/bin/bash
#
# Holt per Authorization-Code-Flow + PKCE ein Access-Token von Keycloak --
# derselbe Flow, den radish-web (der Browser-Client) auch geht, nur ohne
# echten Browser: das Login-Formular wird per curl ausgefuellt.
#
# Nur fuer lokale Testzwecke: Nutzername/Passwort im Klartext an Keycloak zu
# schicken ist genau das, was der Authorization-Code-Flow im echten Client
# vermeiden soll. Hier ist das Absicht -- schneller Weg an ein Token fuer
# curl-Tests der abgesicherten Endpunkte (siehe radish/backend/api/).
#
# Nutzung:
#   ./scripts/get-test-token.sh [username] [password]
# Standard: test-user / test-user (siehe docker/keycloak/realm-radish.json)
#
# Ueberschreibbar per Umgebungsvariable: KEYCLOAK_BASE_URL, REALM, CLIENT_ID,
# REDIRECT_URI, BACKEND_URL.

set -uo pipefail
# Bewusst OHNE "-e": mehrere Schritte pruefen per "if [ -z ... ]" selbst, ob
# ein grep/curl "nichts gefunden" hat -- mit "-e" wuerde die betroffene
# Pipeline (grep findet keine Zeile -> Exit-Code 1, das reicht "pipefail"
# durch) das Skript schon VOR der eigenen Fehlermeldung beenden, und man
# staende ohne jede Erklaerung da. Stattdessen wird nach jedem kritischen
# Schritt explizit auf einen leeren/fehlenden Wert geprueft.

KEYCLOAK_BASE_URL="${KEYCLOAK_BASE_URL:-http://localhost:8081}"
REALM="${REALM:-radish}"
CLIENT_ID="${CLIENT_ID:-radish-web}"
REDIRECT_URI="${REDIRECT_URI:-http://localhost:8000/}"
BACKEND_URL="${BACKEND_URL:-http://localhost:8000}"
USERNAME="${1:-test-user}"
PASSWORD="${2:-test-user}"
CURL_TIMEOUT=20

AUTH_ENDPOINT="$KEYCLOAK_BASE_URL/realms/$REALM/protocol/openid-connect/auth"
TOKEN_ENDPOINT="$KEYCLOAK_BASE_URL/realms/$REALM/protocol/openid-connect/token"

COOKIE_JAR=$(mktemp)
trap 'rm -f "$COOKIE_JAR"' EXIT

echo "==> 1) PKCE-Verifier/Challenge erzeugen" >&2
CODE_VERIFIER=$(openssl rand -base64 96 | tr -d '=+/\n' | cut -c1-64)
CODE_CHALLENGE=$(printf '%s' "$CODE_VERIFIER" | openssl dgst -sha256 -binary | openssl base64 | tr '+/' '-_' | tr -d '=')
STATE=$(openssl rand -hex 16)

echo "==> 2) Login-Seite holen (fuer die Formular-Action-URL)" >&2
LOGIN_PAGE=$(curl -sS --max-time "$CURL_TIMEOUT" -c "$COOKIE_JAR" -G "$AUTH_ENDPOINT" \
  --data-urlencode "client_id=$CLIENT_ID" \
  --data-urlencode "response_type=code" \
  --data-urlencode "scope=openid" \
  --data-urlencode "redirect_uri=$REDIRECT_URI" \
  --data-urlencode "state=$STATE" \
  --data-urlencode "code_challenge=$CODE_CHALLENGE" \
  --data-urlencode "code_challenge_method=S256")
CURL_STATUS=$?
if [ "$CURL_STATUS" -ne 0 ]; then
  echo "Fehler: curl konnte die Login-Seite nicht laden (Exit-Code $CURL_STATUS). Ist Keycloak unter $KEYCLOAK_BASE_URL erreichbar?" >&2
  exit 1
fi

# Keycloaks Login-Formular traegt seine Action-URL HTML-escaped (&amp;) --
# deshalb das Unescaping vor der Weiterverwendung.
FORM_ACTION=$(printf '%s' "$LOGIN_PAGE" | grep -o 'action="[^"]*"' | head -1 | sed -E 's/action="//; s/"$//; s/&amp;/\&/g')

if [ -z "$FORM_ACTION" ]; then
  echo "Fehler: Konnte die Login-Formular-URL nicht finden." >&2
  echo "Ist Keycloak unter $KEYCLOAK_BASE_URL erreichbar, und lief der Realm-Import (Client $CLIENT_ID vorhanden)?" >&2
  echo "--- Antwort von Keycloak ---" >&2
  printf '%s\n' "$LOGIN_PAGE" >&2
  exit 1
fi

echo "==> 3) Login-Formular ausfuellen ($USERNAME)" >&2
# -i statt getrennter -D/-o: Header UND Body landen zusammen in
# LOGIN_RESPONSE, damit wir bei einem Fehlschlag (kein Redirect) sehen
# koennen, was Keycloak stattdessen geschickt hat (Fehlerseite, zusaetzlicher
# Schritt, ...) statt nur "kein Redirect" ohne jeden Kontext.
LOGIN_RESPONSE=$(curl -sS --max-time "$CURL_TIMEOUT" -i -b "$COOKIE_JAR" -c "$COOKIE_JAR" \
  --data-urlencode "username=$USERNAME" \
  --data-urlencode "password=$PASSWORD" \
  "$FORM_ACTION")
CURL_STATUS=$?
if [ "$CURL_STATUS" -ne 0 ]; then
  echo "Fehler: curl konnte das Login-Formular nicht absenden (Exit-Code $CURL_STATUS)." >&2
  exit 1
fi

LOCATION=$(printf '%s' "$LOGIN_RESPONSE" | grep -i '^location:' | tail -1 | sed -E 's/^[Ll]ocation: *//' | tr -d '\r')

if [ -z "$LOCATION" ]; then
  echo "Fehler: Kein Redirect nach dem Login -- Keycloak hat vermutlich einen Fehler oder einen zusaetzlichen Schritt angezeigt statt weiterzuleiten (falsches Passwort? Pflichtaktion wie 'Passwort aendern'?)." >&2
  echo "--- Antwort von Keycloak (Header + Body) ---" >&2
  printf '%s\n' "$LOGIN_RESPONSE" >&2
  exit 1
fi

CODE=$(printf '%s' "$LOCATION" | grep -o 'code=[^&]*' | head -1 | cut -d= -f2)
RETURNED_STATE=$(printf '%s' "$LOCATION" | grep -o 'state=[^&]*' | head -1 | cut -d= -f2)

if [ -z "$CODE" ]; then
  echo "Fehler: Kein 'code' im Redirect gefunden. Redirect-Ziel war: $LOCATION" >&2
  exit 1
fi

if [ "$RETURNED_STATE" != "$STATE" ]; then
  echo "Fehler: state stimmt nicht ueberein -- Abbruch." >&2
  exit 1
fi

echo "==> 4) Code gegen Token tauschen" >&2
TOKEN_RESPONSE=$(curl -sS --max-time "$CURL_TIMEOUT" -X POST "$TOKEN_ENDPOINT" \
  --data-urlencode "grant_type=authorization_code" \
  --data-urlencode "client_id=$CLIENT_ID" \
  --data-urlencode "code=$CODE" \
  --data-urlencode "redirect_uri=$REDIRECT_URI" \
  --data-urlencode "code_verifier=$CODE_VERIFIER")
CURL_STATUS=$?
if [ "$CURL_STATUS" -ne 0 ]; then
  echo "Fehler: curl konnte den Code nicht gegen ein Token tauschen (Exit-Code $CURL_STATUS)." >&2
  exit 1
fi

ACCESS_TOKEN=$(printf '%s' "$TOKEN_RESPONSE" | python3 -c "import json,sys; print(json.load(sys.stdin).get('access_token',''))" 2>/dev/null)

if [ -z "$ACCESS_TOKEN" ]; then
  echo "Fehler: Keycloak hat kein access_token zurueckgegeben." >&2
  echo "--- Antwort von Keycloak ---" >&2
  printf '%s\n' "$TOKEN_RESPONSE" >&2
  exit 1
fi

echo "$TOKEN_RESPONSE" | python3 -m json.tool >&2

echo >&2
# Nur der nackte Token auf stdout -- so laesst sich das Skript auch direkt
# einbetten, z.B. TOKEN=$(./scripts/get-test-token.sh) bzw. in fish:
# set TOKEN (./scripts/get-test-token.sh)
printf '%s\n' "$ACCESS_TOKEN"
