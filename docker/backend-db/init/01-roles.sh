#!/bin/sh
#
# Legt die beiden Benutzer der Backend-Datenbank an (Service "backend-db" in
# docker-compose.yaml, gemountet nach /docker-entrypoint-initdb.d). Das
# Postgres-Image fuehrt das nur auf einem leeren Volume aus, einmal beim
# Anlegen der Datenbank, als POSTGRES_USER.
#
#   radish_owner  Besitzer der Datenbank und damit des Schemas public (ab
#                 Postgres 15 gehoert public dem Besitzer der Datenbank): darf
#                 Tabellen anlegen und aendern. Nur fuer den Migrator
#                 (Service "backend-migrate").
#   radish_app    Darf Daten lesen und schreiben, aber das Schema nicht
#                 aendern. Fuer das Backend (Service "backend").
#
# POSTGRES_USER bleibt Admin der Datenbank -- fuer lokale Entwicklung und
# Tests (config/database.py), nicht fuer die Container.
#
# Die Passwoerter kommen aus der Umgebung des Containers und gehen per
# psql-Variable in die Statements, nicht als Text ins Skript.
set -e

if [ -z "$RADISH_DB_OWNER_PASSWORD" ] || [ -z "$RADISH_DB_APP_PASSWORD" ]; then
    echo "RADISH_DB_OWNER_PASSWORD und RADISH_DB_APP_PASSWORD muessen gesetzt sein." >&2
    exit 1
fi

psql -v ON_ERROR_STOP=1 \
    --username "$POSTGRES_USER" \
    --dbname "$POSTGRES_DB" \
    -v db="$POSTGRES_DB" \
    -v owner_pw="$RADISH_DB_OWNER_PASSWORD" \
    -v app_pw="$RADISH_DB_APP_PASSWORD" <<'EOSQL'
CREATE ROLE radish_owner LOGIN PASSWORD :'owner_pw';
CREATE ROLE radish_app LOGIN PASSWORD :'app_pw';

ALTER DATABASE :"db" OWNER TO radish_owner;

-- Ohne das duerfte sich jeder Benutzer mit der Datenbank verbinden.
REVOKE ALL ON DATABASE :"db" FROM PUBLIC;
GRANT CONNECT ON DATABASE :"db" TO radish_app;
GRANT USAGE ON SCHEMA public TO radish_app;

-- Fuer alles, was radish_owner (also der Migrator) kuenftig anlegt. Djangos
-- Primaerschluessel sind Identity-Spalten; deren Sequenzen brauchen USAGE
-- fuer INSERT, SELECT fuer currval().
ALTER DEFAULT PRIVILEGES FOR ROLE radish_owner IN SCHEMA public
    GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO radish_app;
ALTER DEFAULT PRIVILEGES FOR ROLE radish_owner IN SCHEMA public
    GRANT USAGE, SELECT ON SEQUENCES TO radish_app;
EOSQL
