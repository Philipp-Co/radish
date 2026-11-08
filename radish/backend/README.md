# radish-backend

Django-Backend für radish: Matchmaking-API, Armeen und Katalog, Keycloak-Login,
die technische Schnittstelle zu den Spielinstanzen und der WebSocket, über den
der Browser mit dem Spielserver spricht. Außerdem liefert es das Frontend
(`/`) und den Wasm-Client (`/wasm/`) aus. Wie das Backend mit den übrigen
Teilen zusammenspielt, steht im [Root-README](../../README.md).

## Aufbau

Der Code ist nach Verantwortlichkeit gegliedert: Schnittstellen, Geschäftslogik
und Datenbankmodelle liegen getrennt, und innerhalb der Schnittstellen hat jeder
Bereich ein eigenes Verzeichnis.

```
api/
  interfaces/            HTTP- und WebSocket-Schnittstellen
    urls.py, routing.py  URLs (HTTP) und WebSocket-Routen
    admin/               Adminbereich (Rolle "Radish-Admin"), /api/admin/...
    player/              Spieler (Rolle "Player"), /api/client/... und ws/
    servers/             Spielinstanzen (Rolle "Technischer-User-Server"), /api/servers/...
    auth/                Login/Logout, Token-Prüfung, Keycloak, Realm-Rollen
                         je Bereich: views/, roles/, serializers/
  domain/                Geschäftslogik ohne HTTP
    servers/             Aufrufe an die Spielinstanzen, Zugangscodes
    army_queries.py      Armeen samt Kosten laden
    games.py             Ablauf eines Spiels, den mehrere Schnittstellen teilen
  models/                Datenbankmodelle, eine Datei je Model
  migrations/            Migrationen (generiert)
  management/, fixtures/ seed-Kommando und Dump der Kataloge
  tests/                 Tests, gegliedert wie interfaces/, domain/, models/
config/                  Settings, URLs, ASGI
web/                     Auslieferung von Frontend und Wasm-Client
scripts/                 Hilfsskripte (lizard.sh)
```

## Lokal einrichten

Das Image läuft mit Python 3.14 (`docker/backend/Dockerfile`), lokal ebenso:

```bash
python3.14 -m venv .venv
source .venv/bin/activate      # fish: source .venv/bin/activate.fish
pip install -r requirements-dev.txt
```

`requirements.txt` enthält, was das Backend zur Laufzeit braucht (auch das
Image installiert nur diese Datei), `requirements-dev.txt` zusätzlich die
Werkzeuge aus [Code-Qualität](#code-qualität).

Die Datenbank ist Postgres; lokal reicht der Container aus `docker-compose.yaml`,
er ist unter `localhost:5433` erreichbar (siehe `config/database.py`):

```bash
docker compose up -d backend-db
```

## Tests

```bash
python manage.py test api
```

Die Tests laufen gegen eine eigene Testdatenbank in derselben Postgres-Instanz;
Keycloak und die Spielinstanzen werden nicht gebraucht (Tokens und Aufrufe an die
Instanzen sind in den Tests ersetzt). Die Tests unter `web/` brauchen ein gebautes
Frontend und schlagen ohne es mit 404 fehl.

## Code-Qualität

Neben den Tests prüfen drei Werkzeuge den Code, jedes mit einer eigenen,
versionierten Konfiguration; dazu kommen Architekturtests, die mit den übrigen
Tests laufen (siehe [Architektur mit PyTestArch](#architektur-mit-pytestarch)).
Alle drei laufen aus `radish/backend/` über
`api config web` und sollen ohne Befund durchgehen. Es gibt noch keine CI und
keine pre-commit-Hooks: die Prüfungen werden vor dem Commit von Hand
ausgeführt.

```bash
python manage.py test api
mypy api config web
pylint api config web
scripts/lizard.sh
```

### Stil: Google Python Style Guide

Der Python-Code orientiert sich am
[Google Python Style Guide](https://google.github.io/styleguide/pyguide.html):
Benennung, Importe, Fehlerbehandlung, Type Hints, Kommentare und Aufbau von
Funktionen folgen ihm, solange die Abschnitte unten nichts anderes festlegen.
Die Werkzeuge prüfen davon nur einen Teil; der Rest ist Sache des Reviews.

Bewusste Abweichungen:

- **Zeilenlänge 120** statt 80 Zeichen (`.pylintrc`).
- **Importe einzelner Namen** sind erlaubt, aus eigenem Code wie aus
  Bibliotheken (`from api.models import Game`,
  `from rest_framework.response import Response`) – statt nur Module zu
  importieren und überall `modul.Name` zu schreiben. Importe sind aber immer
  **absolut** (`from api.domain import …`, nie `from ..domain import …`).
- **Docstrings sind keine Pflicht** – weder für Module noch für Klassen oder
  Funktionen, auch nicht für öffentliche. Wo es einen gibt, beschreibt er kurz
  den Zweck; die Abschnitte `Args:`/`Returns:`/`Raises:` werden nicht
  verwendet. Kommentare und Docstrings sind deutsch.
- **Type Hints** gehen über den Guide hinaus: auch jede lokale Variable ist
  annotiert, und `Any` braucht einen Grund (siehe unten).
- Wo Django, DRF oder unittest Namen vorgeben, gelten diese (etwa
  `setUp` oder `assertRejected` im Stil von `assertEqual`).

### Typprüfung mit mypy

Konfiguration: [`mypy.ini`](mypy.ini), mit den Plugins von `django-stubs` und
`djangorestframework-stubs`, damit mypy Models, QuerySets und Serializer
versteht.

- **Strikt:** Jede Funktion braucht vollständige Type Hints für Parameter und
  Rückgabe (`disallow_untyped_defs`, `disallow_incomplete_defs`).
- **Lokale Variablen** sind ebenfalls annotiert (`game: Game | None = ...`).
  Variablen von Schleifen, `with` und Tuple-Zuweisungen werden dafür in einer
  eigenen Zeile davor deklariert:
  ```python
  unit: Unit
  for unit in army.units.all():
      ...
  ```
  Ausgenommen sind nur Stellen, an denen Python keine Annotation erlaubt:
  Comprehension-Variablen, `except ... as e` und Lambda-Parameter. mypy kann
  das nicht erzwingen – es ist eine Konvention, die beim Review geprüft wird.
- **Kein `Any` ohne Grund.** Strukturierte Daten bekommen einen `TypedDict`
  (Spielstart-Dokument, Keycloak-Claims, validierte Serializer-Daten,
  Test-Payloads). `Any` bleibt nur, wo eine Framework-Signatur es vorgibt
  (etwa DRF `validate(attrs)` oder `*args/**kwargs`, die an Django
  durchgereicht werden) – dann mit einem Kommentar `# Any: <Grund>` darüber.
- `cast(...)` und `# type: ignore[...]` nur mit Fehlercode und Kommentar, warum.
  `warn_unused_ignores` meldet ein `type: ignore`, das nichts mehr unterdrückt.
- Die Versionen der Stubs sind auf Django 5.2 abgestimmt (siehe Kommentar in
  `requirements-dev.txt`).

### Statische Analyse mit pylint

Konfiguration: [`.pylintrc`](.pylintrc), mit `pylint-django`.

- Zeilenlänge höchstens **120** Zeichen.
- **Docstrings sind keine Pflicht** (`missing-*-docstring` ist abgeschaltet).
- Abgeschaltet, weil es DRF-Muster sind: `abstract-method` (Eingabe-Serializer
  ohne `create`/`update`) und `too-many-ancestors` (generische Views und Mixins).
- Alles andere ist an. Eine Ausnahme an einer einzelnen Stelle steht als
  `# pylint: disable-next=<meldung>` (oder für eine ganze Datei als
  `# pylint: disable=<meldung>` oben) **mit einer Begründung** in der Zeile
  darüber, zum Beispiel für bewusst gleiche Feldlisten in Admin- und
  Spieler-Serializern (`duplicate-code`) oder die Mock-Parameter in Tests
  (`unused-argument`).

### Komplexität mit lizard

Aufruf und Grenzwerte: [`scripts/lizard.sh`](scripts/lizard.sh). lizard hat keine
Konfigurationsdatei, deshalb stehen die Grenzwerte im Skript und nur dort:

| Kennzahl | Grenze |
|---|---|
| zyklomatische Komplexität (CCN) je Funktion | höchstens 5 |
| Länge einer Funktion | höchstens 500 Zeilen |
| Parameter einer Funktion | höchstens 6 |

Keine Erweiterungen, Migrationen sind ausgenommen. Das Skript gibt nur
Warnungen aus und endet mit einem Exit-Code ungleich 0, sobald eine Grenze
überschritten ist.

Wird eine Funktion zu komplex, wird sie aufgeteilt – typischerweise in kleine
Prüf- oder Hilfsfunktionen (`_check_joinable`, `_validate_weapon`, ...), ohne das
Verhalten zu ändern. Ausnahmen per `# #lizard forgives` sind möglich, werden
bisher aber nicht genutzt.

### Architektur mit PyTestArch

Die Architekturregeln stehen als Tests in
[`api/tests/architecture/test_architecture.py`](api/tests/architecture/test_architecture.py)
und laufen mit `python manage.py test api` – nach dem Vorbild von ArchUnit für
Java, mit [PyTestArch](https://github.com/zyskarch/pytestarch). Trotz des Namens
braucht es kein pytest: eine Regel wirft bei einem Verstoß ein `AssertionError`,
das unittest als fehlgeschlagenen Test meldet, mit dem verbotenen Import in der
Meldung.

| Regel | Grund |
|---|---|
| `models` importiert weder `domain` noch `interfaces` | Schichten: nach unten importieren ist erlaubt, nach oben nicht |
| `domain` importiert nicht `interfaces` | Geschäftslogik kennt kein HTTP |
| `interfaces.admin`, `.player` und `.servers` importieren einander nicht | die Verantwortungsbereiche bleiben getrennt; was mehrere brauchen, gehört nach `domain/` (z.B. `games.end_started_game`) |
| `interfaces.auth` importiert keinen dieser Bereiche | alle Bereiche nutzen auth, nicht umgekehrt |

Eine neue Regel ist eine neue Testmethode in dieser Datei.

### Tests

Die Tests unter `api/tests/` sind wie der Code gegliedert und folgen denselben
Regeln (Type Hints, pylint, lizard). Gemeinsame Helfer, Testkatalog und
Basisklassen liegen in `api/tests/helpers.py`, `army_test_case.py` und
`game_test_case.py`.
