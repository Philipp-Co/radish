"""
Verbindung zur Datenbank, gemeinsam fuer config/settings.py (Backend) und
config/settings_migrate.py (Migrator, siehe docker/backend-migrate/). Steht
hier statt in settings.py, damit der Migrator settings.py nicht importieren
muss -- das verlangt im Produktionsmodus einen DJANGO_SECRET_KEY und liest
Werte (Keycloak, HTTPS), die fuer migrate keine Rolle spielen.

Postgres statt sqlite (siehe docker-compose.yaml, Services "backend",
"backend-migrate" und "backend-db"). Alle Werte per Umgebungsvariable
ueberschreibbar; die Defaults passen zum "backend-db"-Service, der von aussen
(ausserhalb von Docker, z.B. fuer ein lokales `manage.py`) unter
localhost:5433 erreichbar ist -- deshalb DJANGO_DB_HOST default "localhost",
nicht "backend-db" (das ist nur der Servicename im Docker-Netzwerk selbst).

Der Default-Benutzer "radish" ist der Admin der Datenbank (POSTGRES_USER in
docker-compose.yaml) und nur fuer lokale Entwicklung und Tests gedacht. Im
Container bekommen Migrator und Backend je einen eigenen Benutzer mit
eingeschraenkten Rechten (siehe docker/backend-db/init/01-roles.sql).

Die Tabellen liegen im Schema DJANGO_DB_SCHEMA (Default "radish"), nicht in
public: die Anwendung setzt dafuer bei jeder Verbindung ihren search_path
selbst. Anlegen muss das Schema vorher die Datenbank -- das tut
docker/backend-db/init/01-roles.sql (RADISH_DB_SCHEMA dort, beide Namen
muessen zusammenpassen). Fehlt es, scheitert schon das erste `migrate`.
"""

import os
import re

from django.core.exceptions import ImproperlyConfigured

# Nur einfache Bezeichner: der Name geht unquotiert in den search_path.
_SCHEMA_NAME = re.compile(r'[a-z_][a-z0-9_]*')


def database(options: dict[str, object] | None = None) -> dict[str, object]:
    """Eintrag fuer DATABASES['default']; options ergaenzt dessen OPTIONS."""
    schema = os.environ.get('DJANGO_DB_SCHEMA', 'radish')
    if not _SCHEMA_NAME.fullmatch(schema):
        raise ImproperlyConfigured(f"DJANGO_DB_SCHEMA ist kein gueltiger Schemaname: {schema!r}")
    return {
        'ENGINE': 'django.db.backends.postgresql',
        'NAME': os.environ.get('DJANGO_DB_NAME', 'radish'),
        'USER': os.environ.get('DJANGO_DB_USER', 'radish'),
        'PASSWORD': os.environ.get('DJANGO_DB_PASSWORD', 'radish'),
        'HOST': os.environ.get('DJANGO_DB_HOST', 'localhost'),
        'PORT': os.environ.get('DJANGO_DB_PORT', '5433'),
        'OPTIONS': {
            # Startparameter der Verbindung (libpq "options"): gilt fuer
            # jede Verbindung, auch die des Pools, ohne eigenes SET.
            'options': f'-c search_path={schema}',
            **(options or {}),
        },
    }
