import unittest

from django.apps import AppConfig, apps
from django.db.migrations.loader import MigrationLoader

from config import settings_migrate


class MigratorSettingsTests(unittest.TestCase):
    """config/settings_migrate.py: der Migrator kennt jede App mit Migrationen."""

    def test_every_app_with_migrations_is_migrated(self) -> None:
        migrated: set[str] = {AppConfig.create(entry).label for entry in settings_migrate.INSTALLED_APPS}
        # Ohne Verbindung: nur die Migrationsdateien der installierten Apps.
        with_migrations: set[str] = MigrationLoader(None).migrated_apps

        # Sonst legt der Migrator die Tabellen dieser Apps nie an, und das
        # Backend bricht beim Start ab (docker/backend/entrypoint.sh).
        self.assertEqual(with_migrations - migrated, set())
        # Umgekehrt keine App, die das Backend gar nicht hat.
        self.assertLessEqual(migrated, {a.label for a in apps.get_app_configs()})
