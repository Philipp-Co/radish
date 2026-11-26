"""
`manage.py seed`: fuellt die Datenbank mit den Katalogen aus
api/fixtures/seed.sql (Spezies, Einheitentypen, Profile, Waffen,
Ausruestung, Gelaende mit seinen Bildern, Einheitenbilder, Effekte). Die
Migrationen enthalten nur das Schema; was in einer neuen Datenbank stehen
soll, kommt von hier. Benutzer,
Spieler, Armeen und Spiele entstehen im Betrieb und sind nicht im Dump.

Nur von Hand, als Aufgabe eines Admins (README, Abschnitt "Startdaten"):
weder der Migrator noch das Backend laden den Dump, und er liegt in keinem
Image. Typisch einmal nach dem ersten `docker compose up` einer neuen
Umgebung, von einem Rechner mit dem Repository aus gegen deren Datenbank
(DJANGO_DB_*, config/database.py).

Geladen wird nur in leere Tabellen: hat eine der Tabellen aus dem Dump schon
Zeilen, bricht das Kommando ab, ohne etwas zu schreiben. Ein SQL-Dump fuegt
Zeilen ein, er ueberschreibt keine, und Armeen verweisen auf die Kataloge --
wer neu befuellen will, legt die Datenbank neu an.

Mit --table (mehrfach) nur diese Tabellen des Dumps samt ihren Sequenzen,
und geprueft werden nur sie -- etwa um einer gepflegten Datenbank die
Einheitenbilder (api_unitsprite, api_unitspriteimage, api_unitspriteatlas)
oder die Tile-Bilder (api_terrainimage, api_terraindecor) oder die Effekte
(api_effect, api_effectimage, api_effectatlas) nachzuliefern. Was
die Tabellen untereinander verweisen, muss dann passen.

Den Dump erzeugt scripts/db-dump-seed.sh, ebenfalls von Hand, mit pg_dump
--data-only aus einer gepflegten Datenbank. Gelesen wird er hier statt mit
psql -- so braucht es keine Postgres-Werkzeuge, und es laeuft nur, was ein solcher
Dump enthalten darf: SET, SELECT pg_catalog.setval(...) und COPY ... FROM
stdin. Alles andere bricht ab. Die Tabellen stehen ohne Schema darin und
landen im search_path der Verbindung (DJANGO_DB_SCHEMA, config/database.py);
alles in einer Transaktion.
"""

from argparse import ArgumentParser
from collections.abc import Iterator
from dataclasses import dataclass, field
from pathlib import Path
import re
from typing import Any, TextIO

from django.core.management.base import BaseCommand, CommandError
from django.db import connection, transaction
from django.db.backends.utils import CursorWrapper

FIXTURE = Path(__file__).resolve().parent.parent.parent / "fixtures" / "seed.sql"

# Wie pg_dump sie schreibt, nach scripts/db-dump-seed.sh ohne Schema-Praefix.
_COPY = re.compile(r"^COPY (?P<table>[a-z_][a-z0-9_]*) \([a-z0-9_, ]+\) FROM stdin;$")
_SET = re.compile(r"^SET [a-z_]+ = [^;]+;$")
_SETVAL = re.compile(r"^SELECT pg_catalog\.setval\('(?P<sequence>[a-z_][a-z0-9_]*)', \d+, (true|false)\);$")
_END_OF_COPY = "\\.\n"


@dataclass
class Statement:
    """Eine Anweisung des Dumps; bei COPY mit ihren Zeilen (Textformat von COPY)."""

    sql: str
    table: str | None = None
    rows: list[str] = field(default_factory=list)
    sequence: str | None = None

    @property
    def owner(self) -> str | None:
        """
        Die Tabelle, zu der die Anweisung gehoert: die des COPY oder die der
        Sequenz (Django nennt sie "<tabelle>_id_seq"); None bei SET.
        """
        if self.sequence is not None:
            return self.sequence.removesuffix("_id_seq")
        return self.table

    def belongs_to(self, tables: list[str]) -> bool:
        """Ob die Anweisung zu "tables" gehoert (leer: zu allen); SET gehoert immer dazu."""
        return not tables or self.owner is None or self.owner in tables


class Command(BaseCommand):
    help = "Fuellt leere Katalog-Tabellen aus api/fixtures/seed.sql (nur von Hand)."

    # Wie migrate im Migrator (--skip-checks): mit config.settings_migrate
    # verlangen die Checks des Admins Templates und Middleware, die es dort
    # nicht gibt.
    requires_system_checks = []

    def add_arguments(self, parser: ArgumentParser) -> None:
        parser.add_argument("--table", action="append", default=[], help="nur diese Tabelle des Dumps (mehrfach)")

    # Any: Signatur von BaseCommand.handle in den django-stubs; options kommt
    # ungeprueft aus argparse/call_command (z.B. options["verbosity"]).
    def handle(self, *args: Any, **options: Any) -> None:
        tables: list[str] = checked_tables(options.get("table") or [])
        filled: list[str] = filled_tables(tables)
        if filled:
            raise CommandError(f"Nichts geladen: schon Inhalt in {', '.join(filled)}.")

        rows: int = load_fixture(tables)
        self.stdout.write(f"Seed: {rows} Zeilen aus {FIXTURE.name} geladen.")


def checked_tables(tables: list[str]) -> list[str]:
    """Die mit --table genannten Tabellen, wenn der Dump sie alle kennt."""
    unknown: list[str] = sorted(set(tables) - set(tables_in_fixture()))
    if unknown:
        raise CommandError(f"Nicht im Dump: {', '.join(unknown)}.")
    return tables


def filled_tables(tables: list[str] | None = None) -> list[str]:
    """Die Tabellen aus dem Dump ("tables", leer: alle), die schon Zeilen haben."""
    filled: list[str] = []
    with connection.cursor() as cursor:
        table: str
        for table in tables or tables_in_fixture():
            cursor.execute(f"SELECT EXISTS (SELECT 1 FROM {connection.ops.quote_name(table)})")
            if cursor.fetchone()[0]:
                filled.append(table)
    return filled


def tables_in_fixture() -> list[str]:
    """Die Tabellen, in die der Dump Zeilen schreibt, alphabetisch."""
    return sorted({statement.table for statement in read_fixture() if statement.table is not None})


def read_fixture() -> list[Statement]:
    """Die Anweisungen des Dumps, samt der Zeilen jedes COPY."""
    f: TextIO
    with FIXTURE.open(encoding="utf-8") as f:
        return list(_statements(f))


def load_fixture(tables: list[str] | None = None) -> int:
    """
    Fuehrt den Dump ("tables", leer: ganz) in einer Transaktion aus; die Zahl
    der geladenen Zeilen.
    """
    count: int = 0
    with transaction.atomic(), connection.cursor() as cursor:
        statement: Statement
        for statement in read_fixture():
            if statement.belongs_to(tables or []):
                count += _execute(cursor, statement)
    return count


def _execute(cursor: CursorWrapper, statement: Statement) -> int:
    """Fuehrt eine Anweisung aus; die Zahl der Zeilen, die sie einfuegt."""
    if statement.table is None:
        cursor.execute(statement.sql)
        return 0
    # Djangos CursorWrapper kennt COPY nicht; der psycopg-Cursor darunter schon.
    with cursor.cursor.copy(statement.sql) as copy:
        row: str
        for row in statement.rows:
            copy.write(row)
    return len(statement.rows)


def _statements(lines: TextIO) -> Iterator[Statement]:
    """Zerlegt den Dump; alles, was ein Daten-Dump nicht enthaelt, ist ein Fehler."""
    number: int
    line: str
    numbered: Iterator[tuple[int, str]] = enumerate(lines, start=1)
    for number, line in numbered:
        text: str = line.rstrip("\n")
        if text and not text.startswith("--"):
            yield _statement(text, number, numbered)


def _statement(text: str, number: int, numbered: Iterator[tuple[int, str]]) -> Statement:
    """Eine Zeile des Dumps als Anweisung; bei COPY liest sie dessen Zeilen aus "numbered" mit."""
    copy: re.Match[str] | None = _COPY.match(text)
    if copy is not None:
        return Statement(sql=text.removesuffix(";"), table=copy["table"], rows=_copy_rows(numbered, number))
    if _SET.match(text):
        # LOCAL: gilt nur bis zum Ende der Transaktion, nicht fuer die Verbindung.
        return Statement(sql="SET LOCAL " + text.removeprefix("SET "))
    setval: re.Match[str] | None = _SETVAL.match(text)
    if setval is not None:
        return Statement(sql=text, sequence=setval["sequence"])
    raise CommandError(f"{FIXTURE.name}:{number}: unerwartete Anweisung: {text[:80]}")


def _copy_rows(numbered: Iterator[tuple[int, str]], start: int) -> list[str]:
    """Die Zeilen eines COPY bis zu seinem Ende (eine Zeile nur mit "\\.")."""
    rows: list[str] = []
    line: str
    for _, line in numbered:
        if line == _END_OF_COPY:
            return rows
        rows.append(line)
    raise CommandError(f"{FIXTURE.name}:{start}: COPY ohne Ende.")
