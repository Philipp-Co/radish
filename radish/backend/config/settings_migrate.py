"""
Settings fuer den Migrator (docker/backend-migrate/Dockerfile, Service
"backend-migrate" in docker-compose.yaml): nur, was `migrate` braucht.

Bewusst ohne Import von config/settings.py -- das verlangt im
Produktionsmodus einen DJANGO_SECRET_KEY und haengt an Paketen (daphne,
channels, rest_framework), die das Migrator-Image nicht mitbringt.

INSTALLED_APPS enthaelt genau die Apps mit Migrationen. Fehlt hier eine, die
config/settings.py hat, legt der Migrator ihre Tabellen nie an und das
Backend bricht beim Start ab (`migrate --check` in
docker/backend/entrypoint.sh) -- api/tests.py.MigratorSettingsTests prueft
das schon vorher.
"""

from .database import database

INSTALLED_APPS = [
    # SimpleAdminConfig statt 'django.contrib.admin': ohne Autodiscover, das
    # sonst die admin.py aller Apps importiert -- api/admin.py liegt nicht im
    # Image. Die App selbst braucht es fuer ihre Tabelle django_admin_log.
    'django.contrib.admin.apps.SimpleAdminConfig',
    'django.contrib.auth',
    'django.contrib.contenttypes',
    'django.contrib.sessions',
    'api',
]

DATABASES = {
    'default': database(),
}

# Wie in config/settings.py -- beides bestimmt, welche Spalten die
# Migrationen anlegen.
USE_TZ = True
TIME_ZONE = 'UTC'
DEFAULT_AUTO_FIELD = 'django.db.models.BigAutoField'
