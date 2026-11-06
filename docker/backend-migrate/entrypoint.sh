#!/bin/sh
#
# Einstiegspunkt des Migrators: erst das Schema (migrate), dann je nach
# RADISH_DB_SEED die Inhalte aus api/fixtures/seed.json (siehe
# radish/backend/api/management/commands/seed.py): never, if-empty (Default)
# oder always. Schlaegt eins davon fehl, endet der Container mit Fehler und
# "backend" startet nicht (siehe docker-compose.yaml).
#
# --skip-checks bei migrate: die System-Checks des Admins verlangen Templates,
# Messages und Middleware, die es hier nicht gibt. Gecheckt wird im Backend.
set -e

django-admin migrate --noinput --skip-checks
exec django-admin seed --mode "${RADISH_DB_SEED:-if-empty}"
