#!/bin/sh
#
# Einstiegspunkt des Migrators: nur das Schema (migrate). Schlaegt es fehl,
# endet der Container mit Fehler und "backend" startet nicht (siehe
# docker-compose.yaml).
#
# Inhalte laedt der Migrator bewusst nicht: die Kataloge aus
# api/fixtures/seed.sql laedt ein Admin von Hand (manage.py seed, siehe
# README, Abschnitt "Startdaten").
#
# --skip-checks bei migrate: die System-Checks des Admins verlangen Templates,
# Messages und Middleware, die es hier nicht gibt. Gecheckt wird im Backend.
set -e

exec django-admin migrate --noinput --skip-checks
