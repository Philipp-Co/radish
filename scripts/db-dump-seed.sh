#!/bin/sh
#
# Schreibt die Kataloge der Backend-Datenbank nach
# radish/backend/api/fixtures/seed.json -- dem Dump, aus dem der Migrator eine
# neue Datenbank fuellt (siehe radish/backend/api/management/commands/seed.py).
# Danach die Aenderungen an seed.json pruefen und committen.
#
# Nur die Kataloge, die ein Admin pflegt: Spezies, Einheitentypen mit ihren
# Profilen, Waffen, Ausruestung und Gelaende. Sie verweisen nur
# untereinander. Nicht dabei sind die Daten, die im Betrieb entstehen --
# Benutzer und Spieler (kommen ueber Keycloak), Armeen, Spiele und die
# Spielserver-Instanzen (melden sich selbst an) -- und alles, was migrate
# selbst anlegt (contenttypes, Permissions).
#
# Laeuft mit einer lokalen Python-Umgebung (pip install -r
# radish/backend/requirements-migrate.txt) gegen "backend-db" auf
# localhost:5433, als Admin der Datenbank. Andere Datenbank ueber DJANGO_DB_*
# wie in radish/backend/config/database.py.
set -e

cd "$(dirname "$0")/../radish/backend"

DJANGO_SETTINGS_MODULE=config.settings_migrate PYTHONPATH=. python -m django dumpdata \
    --skip-checks \
    --indent 2 \
    api.species \
    api.unittype \
    api.entityprofile \
    api.weapon \
    api.equipment \
    api.terraintype \
    -o api/fixtures/seed.json
