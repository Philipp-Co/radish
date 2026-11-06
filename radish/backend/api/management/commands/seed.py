"""
`manage.py seed`: fuellt die Datenbank mit den Katalogen aus
api/fixtures/seed.json (Spezies, Einheitentypen, Profile, Waffen,
Ausruestung, Gelaende). Die Migrationen enthalten nur das Schema; was in
einer neuen Datenbank stehen soll, kommt von hier. Benutzer, Spieler, Armeen
und Spiele entstehen im Betrieb und sind nicht im Dump.

Laeuft im Migrator direkt nach migrate (docker/backend-migrate/entrypoint.sh),
gesteuert ueber RADISH_DB_SEED in docker-compose.yaml:

  never     nichts laden
  if-empty  nur laden, wenn keine der Katalog-Tabellen aus dem Dump schon
            Zeilen hat -- also in einer neuen Datenbank. Danach gilt, was der
            Admin pflegt.
  always    jedes Mal laden. loaddata schreibt jeden Datensatz unter seinem
            Primaerschluessel und ueberschreibt damit, was dort steht; was
            nicht im Dump ist, bleibt. Nur fuer Test-/Demo-Umgebungen.

Den Dump selbst erzeugt scripts/db-dump-seed.sh aus einer gepflegten
Datenbank.
"""

import json
from pathlib import Path

from django.apps import apps
from django.core.management import call_command
from django.core.management.base import BaseCommand

FIXTURE = Path(__file__).resolve().parent.parent.parent / "fixtures" / "seed.json"

MODES = ("never", "if-empty", "always")


class Command(BaseCommand):
    help = "Fuellt die Datenbank aus api/fixtures/seed.json (never/if-empty/always)."

    # Wie migrate im Migrator (--skip-checks): die Checks des Admins verlangen
    # Templates und Middleware, die es dort nicht gibt.
    requires_system_checks = []

    def add_arguments(self, parser):
        parser.add_argument("--mode", choices=MODES, default="if-empty")

    def handle(self, *args, mode, **options):
        if mode == "never":
            self.stdout.write("Seed: uebersprungen (never).")
            return

        if mode == "if-empty":
            filled = [label for label in models_in_fixture() if apps.get_model(label).objects.exists()]
            if filled:
                self.stdout.write(f"Seed: uebersprungen, schon Inhalt in {', '.join(filled)}.")
                return

        call_command("loaddata", str(FIXTURE), verbosity=options["verbosity"])


def models_in_fixture():
    """Die Models ("app.model"), von denen der Dump Datensaetze enthaelt."""
    with FIXTURE.open(encoding="utf-8") as f:
        return sorted({entry["model"] for entry in json.load(f)})
