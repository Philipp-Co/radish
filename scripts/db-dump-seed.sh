#!/bin/sh
#
# Schreibt die Kataloge der Backend-Datenbank als SQL-Dump nach
# radish/backend/api/fixtures/seed.sql -- dem Dump, aus dem ein Admin eine
# neue Datenbank von Hand fuellt (manage.py seed, siehe
# radish/backend/api/management/commands/seed.py). Nur von Hand, nichts ruft
# das Skript automatisch auf. Danach die Aenderungen an seed.sql pruefen und
# committen.
#
# Nur die Kataloge, die ein Admin pflegt: Spezies, Einheitentypen mit ihren
# Profilen, Waffen, Ausruestung, Gelaende mit seinen Bildern (TerrainImage,
# TerrainDecor), die Einheitenbilder (UnitSprite mit seinen PNGs und
# Atlanten) und die Effekte (Effect mit seinen PNGs und Atlanten). Die PNGs
# stehen als bytea in Hex im Dump, es gibt sie sonst nirgends: der Client-Build
# bringt weder Einheiten- noch Tile- noch Effekt-Bilder mit.
# Sie verweisen nur untereinander. Nicht dabei sind die Daten, die im Betrieb
# entstehen -- Benutzer und Spieler (kommen ueber Keycloak), Armeen, Spiele
# und die Spielserver-Instanzen (melden sich selbst an) -- und alles, was
# migrate selbst anlegt (contenttypes, Permissions).
#
# Nur Daten (--data-only), kein Schema: das kommt weiter aus den
# Django-Migrationen, der Migrator laedt den Dump erst nach migrate. Nach einer
# Migration, die eine Katalog-Tabelle aendert, den Dump deshalb neu schreiben.
#
# pg_dump laeuft im Container "backend-db" (docker compose exec) -- dieselbe
# Version wie der Server, lokal braucht es keine Postgres-Werkzeuge. Aufruf aus
# einem beliebigen Verzeichnis, solange "backend-db" laeuft. Andere Datenbank
# oder anderes Schema ueber DJANGO_DB_NAME / DJANGO_DB_SCHEMA wie in
# radish/backend/config/database.py.
#
# Nachbearbeitet, damit der Dump in jedes Schema passt und gleiche Daten
# gleiche Bytes ergeben:
#   - ohne die Schema-Praefixe bei COPY und setval und ohne das Leeren des
#     search_path -- seed.py laedt in den search_path der Verbindung
#     (DJANGO_DB_SCHEMA);
#   - ohne \restrict/\unrestrict (psql-Befehle mit zufaelligem Schluessel, seit
#     pg_dump 16.10) -- seed.py liest den Dump selbst, nicht psql.
set -e

DB_NAME="${DJANGO_DB_NAME:-radish}"
DB_SCHEMA="${DJANGO_DB_SCHEMA:-radish}"

cd "$(dirname "$0")/.."

OUT=radish/backend/api/fixtures/seed.sql
RAW="$OUT.tmp"

# In eine eigene Datei, bevor seed.sql ueberschrieben wird: scheitert pg_dump,
# bleibt der alte Dump stehen.
docker compose exec -T backend-db pg_dump \
    --username radish \
    --dbname "$DB_NAME" \
    --data-only \
    --table "$DB_SCHEMA.api_species" \
    --table "$DB_SCHEMA.api_unittype" \
    --table "$DB_SCHEMA.api_entityprofile" \
    --table "$DB_SCHEMA.api_weapon" \
    --table "$DB_SCHEMA.api_weapon_unit_types" \
    --table "$DB_SCHEMA.api_weapon_profiles" \
    --table "$DB_SCHEMA.api_equipment" \
    --table "$DB_SCHEMA.api_equipment_unit_types" \
    --table "$DB_SCHEMA.api_equipment_profiles" \
    --table "$DB_SCHEMA.api_terraintype" \
    --table "$DB_SCHEMA.api_terrainimage" \
    --table "$DB_SCHEMA.api_terraindecor" \
    --table "$DB_SCHEMA.api_unitsprite" \
    --table "$DB_SCHEMA.api_unitspriteimage" \
    --table "$DB_SCHEMA.api_unitspriteatlas" \
    --table "$DB_SCHEMA.api_effect" \
    --table "$DB_SCHEMA.api_effectimage" \
    --table "$DB_SCHEMA.api_effectatlas" \
    > "$RAW"

sed \
    -e '/^\\restrict /d' \
    -e '/^\\unrestrict /d' \
    -e "/^SELECT pg_catalog.set_config('search_path', '', false);\$/d" \
    -e "s/^COPY $DB_SCHEMA\\./COPY /" \
    -e "s/^SELECT pg_catalog.setval('$DB_SCHEMA\\./SELECT pg_catalog.setval('/" \
    "$RAW" > "$OUT"
rm "$RAW"
