#!/bin/sh
#
# Migrationen vor dem eigentlichen Start (siehe DATABASES in settings.py --
# Postgres via DJANGO_DB_HOST/... aus docker-compose.yaml).
#
# "depends_on" in docker-compose sorgt nur dafuer, dass der
# Postgres-Container VOR diesem hier STARTET -- nicht, dass er beim Start
# dieses Containers auch schon Verbindungen annimmt (Postgres braucht nach
# dem Start noch etwas Zeit fuer initdb/Recovery). Deshalb hier aktiv auf
# eine echte DB-Verbindung warten, bevor migrate versucht wird.
#
set -e

python3 - <<'PYEOF'
import os
import sys
import time

import psycopg2

host = os.environ.get("DJANGO_DB_HOST", "localhost")
port = os.environ.get("DJANGO_DB_PORT", "5433")
name = os.environ.get("DJANGO_DB_NAME", "radish")
user = os.environ.get("DJANGO_DB_USER", "radish")
password = os.environ.get("DJANGO_DB_PASSWORD", "radish")

deadline = time.time() + 30
last_error = None
while time.time() < deadline:
    try:
        conn = psycopg2.connect(host=host, port=port, dbname=name, user=user, password=password)
        conn.close()
        sys.exit(0)
    except psycopg2.OperationalError as exc:
        last_error = exc
        time.sleep(1)

print(f"Postgres unter {host}:{port} nach 30s nicht erreichbar: {last_error}", file=sys.stderr)
sys.exit(1)
PYEOF

python manage.py migrate --noinput

exec python manage.py runserver 0.0.0.0:8000
