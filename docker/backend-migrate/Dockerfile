# Migrator fuer die Datenbank des Django-Backends (radish/backend/): fuehrt
# einmal `migrate` aus, fuellt je nach RADISH_DB_SEED die Inhalte aus
# api/fixtures/seed.json ein (siehe entrypoint.sh) und endet. Laeuft beim
# Deployment vor dem Backend
# (Service "backend-migrate" in docker-compose.yaml); das Backend selbst
# aendert das Schema nicht mehr, sondern prueft beim Start nur, dass nichts
# fehlt (siehe docker/backend/entrypoint.sh).
#
# Bewusst nur, was `migrate` braucht -- kein Frontend, kein Wasm-Client, keine
# Views/Consumer, kein daphne/channels/rest_framework. Die Models und
# Migrationen von api/ importieren nur Django; die Settings dafuer stehen in
# config/settings_migrate.py. Alle Abhaengigkeiten gibt es als fertige
# musllinux-Wheels, deshalb ohne eigene Builder-Stage (anders als
# docker/backend/Dockerfile, das fuer simplejwt[crypto] Rust braucht).
#
# Build-Kontext ist wie beim Backend die Wurzel des Repositorys (siehe
# .dockerignore).
FROM python:3.12-alpine

COPY radish/backend/requirements-migrate.txt /tmp/requirements-migrate.txt
RUN pip install --no-cache-dir -r /tmp/requirements-migrate.txt \
    && rm /tmp/requirements-migrate.txt

WORKDIR /app

# Wie im Backend (siehe docker/backend/Dockerfile): sonst kommt die Ausgabe
# von migrate verzoegert in "docker compose logs" an.
ENV PYTHONUNBUFFERED=1
ENV PYTHONPATH=/app
ENV DJANGO_SETTINGS_MODULE=config.settings_migrate

# Einzeln statt ganzer Verzeichnisse: neue Dateien in config/ oder api/
# sollen nicht ungefragt im Image landen.
COPY radish/backend/config/__init__.py radish/backend/config/database.py radish/backend/config/settings_migrate.py /app/config/
COPY radish/backend/api/__init__.py radish/backend/api/apps.py radish/backend/api/models.py /app/api/
COPY radish/backend/api/migrations/ /app/api/migrations/
COPY radish/backend/api/management/__init__.py /app/api/management/
COPY radish/backend/api/management/commands/__init__.py radish/backend/api/management/commands/seed.py /app/api/management/commands/
COPY radish/backend/api/fixtures/seed.json /app/api/fixtures/
COPY docker/backend-migrate/entrypoint.sh /entrypoint.sh
# Nicht als root; der Migrator schreibt nichts ins Dateisystem. COPY
# uebernimmt die Rechte aus dem Arbeitsverzeichnis, dort sind manche Dateien
# nur fuer ihren Besitzer lesbar (siehe docker/backend/Dockerfile).
RUN addgroup -S radish && adduser -S -D -H -G radish radish \
    && chmod -R a+rX /app \
    && chmod 755 /entrypoint.sh
USER radish

ENTRYPOINT ["/entrypoint.sh"]
