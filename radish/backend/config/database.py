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
eingeschraenkten Rechten (siehe docker/backend-db/init/01-roles.sh).
"""

import os


def database(**extra: object) -> dict[str, object]:
    """Eintrag fuer DATABASES['default']; extra ergaenzt ihn (z.B. OPTIONS)."""
    return {
        'ENGINE': 'django.db.backends.postgresql',
        'NAME': os.environ.get('DJANGO_DB_NAME', 'radish'),
        'USER': os.environ.get('DJANGO_DB_USER', 'radish'),
        'PASSWORD': os.environ.get('DJANGO_DB_PASSWORD', 'radish'),
        'HOST': os.environ.get('DJANGO_DB_HOST', 'localhost'),
        'PORT': os.environ.get('DJANGO_DB_PORT', '5433'),
        **extra,
    }
