# game-server

Django-Projekt fuer die Betriebs-Oberflaeche der Game-Server-Instanzen, die
auf diesem Host laufen. Nicht zu verwechseln mit
[`radish/backend/`](../backend/): das ist die Matchmaking-API
(`Game`/`GameServer`/`Player`), die nur *verwaltet*, wer mit wem in welchem
Spiel steckt. Dieses Projekt hier soll die dahinter stehenden Prozesse
tatsaechlich starten, stoppen und ueberwachen koennen -- also
`zucchini_server` + `radish_server` (siehe
[`radish/game-server-core/`](../game-server-core/)), jeweils ein Paar pro
Instanz.

## Stand

Die App `instances` stellt die Endpunkte bereit, ueber die das Backend die
Instanzen steuert (siehe unten). Startet der Host ein Spiel aus der Lobby,
ruft das Backend `game/start/` auf. Der Aufruf ist synchron: die Instanz setzt
das Spiel noch im Request auf (eine Datei schreiben, den C-Prozess darueber
informieren) und antwortet erst danach mit 200 -- dann laeuft das Spiel, und
die Spieler verlassen die Lobby.

Aufsetzen heisst: Django legt die Spieldaten als Datei ab und weckt
`radish_server` per `SIGUSR1`. Je Instanz gibt es dafuer ein Verzeichnis:

```
$RADISH_GAME_DATA_DIR/<instanz>/      z.B. /run/radish/game-instance-1/
├── spielstart.json     schreibt Django (atomar: .tmp, dann umbenennen)
└── radish_server.pid   schreibt radish_server, sobald sein SIGUSR1-Handler steht
```

Angelegt und beim Start der Instanz geleert wird es von
[`game-instance-entrypoint.sh`](../../docker/game-server/game-instance-entrypoint.sh),
das `radish_server` auch die beiden Pfade mitgibt (`RADISH_GAME_START_PATH`,
`RADISH_GAME_PID_PATH`). Auf das Signal liest `radish_server` die Datei neu
und schreibt ins Log, wer mit welcher Armee antritt; fehlt sie, ist kein
Spiel mehr angesetzt. Der Abbruch loescht die Datei und schickt dasselbe
Signal.

**Keine Rueckmeldung des C-Prozesses:** 200 heisst "Datei liegt, Prozess ist
benachrichtigt", nicht "Prozess hat sie gelesen". Nur wenn es gar keinen
laufenden `radish_server` gibt (PID-Datei fehlt oder ist veraltet), antwortet
`game/start/` mit 500 -- das Spiel bleibt dann in der Lobby.

Kein Modell, keine Auth an den eigenen Endpunkten; die Instanzverwaltung
(starten, stoppen ueber supervisord) kommt in einem spaeteren Schritt. Unter
welcher URL das Backend diese Endpunkte erreicht, meldet die Instanz beim
Registrieren mit (`RADISH_GAME_SERVER_CONTROL_URL`, siehe
[`docker-compose.yaml`](../../docker-compose.yaml)).

## Endpunkte

Der Body von `game/start/` sind die Spieldaten nach
[`radish/game/schema/spielstart.schema.json`](../game/schema/spielstart.schema.json):
beide Spieler (Keycloak-Name, Spieler-Kennung) mit ihren Armeen samt allen
Katalogwerten -- die Instanz braucht keinen Zugriff aufs Backend.

Django REST Framework, klassenbasierte Views in
[`instances/views.py`](instances/views.py). `<name>` ist der Name der Instanz in
diesem Container, wie `docker/game-server/supervisord.conf` ihn vergibt (z.B.
`game-instance-1`) -- zugleich ihr Zucchini-Name. Das Backend kennt ihn als
`GameServer.instance_name`; sein `GameServer.name` ist
`<RADISH_GAME_SERVER_ID>-<name>`, damit er ueber alle Container eindeutig ist.

| Methode | Pfad                                  | View             | Antwort |
|---------|---------------------------------------|------------------|---------|
| POST    | `/api/instances/<name>/game/start/`   | `GameStartView`  | 200 + `{"instance": ..., "status": "ready"}`, 400 ohne zwei Spieler unter `spieldaten` oder ohne Liste unter `gelaende`, 500 wenn die Datei nicht zu schreiben ist oder kein `radish_server` laeuft |
| POST    | `/api/instances/<name>/game/cancel/`  | `GameCancelView` | 204, loescht die Spielstart-Datei und benachrichtigt `radish_server` |

Ohne Authentifizierung und ohne CSRF-Pruefung -- nur fuer Dev, die Auth
kommt in `InstanceAPIView` hinzu.

Der Container selbst faehrt aber schon Django und die Game-Instanzen
nebeneinander und haelt alle am Leben: [`supervisord`](../../docker/game-server/supervisord.conf)
uebernimmt die Rolle von "etwas wie systemd" -- stuerzt Django oder eines der Paare
`zucchini_server`/`radish_server` ab, startet supervisord neu, was abgestuerzt
ist (siehe [`game-instance-entrypoint.sh`](../../docker/game-server/game-instance-entrypoint.sh)
fuer die Prozesspaarung der beiden C-Programme).

Vier Instanzen, `game-instance-1` bis `-4` auf den UDP-Ports 9999 bis 10002.
Name und Port je Instanz stehen als Paar in `supervisord.conf`
(`game-instance-entrypoint.sh <name> <port>`); jede meldet sich einzeln beim
Backend an. Eine weitere Instanz ist eine weitere
`[program:game-instance-N]`-Sektion mit dem naechsten Port, dazu die Spanne
in `EXPOSE` (Dockerfile) und `ports:` (`docker-compose.yaml`).

## Lokal starten

```bash
cd radish/game-server
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python manage.py runserver
```

Fuer die Werkzeuge aus [Code-Qualitaet](#code-qualitaet) statt
`requirements.txt` die `requirements-dev.txt` installieren.

Danach ist die Instanz-API unter <http://localhost:8000/api/> erreichbar. Eine
Admin-Oberflaeche gibt es nicht: game-server hat keine eigenen Models, und die
API ruft nur das Backend auf.

Tests:

```bash
python manage.py test instances
```

## Ueber Docker

```bash
docker compose up --build game-server
```

Danach ist die Instanz-API unter <http://localhost:8090/api/> erreichbar
(Port-Mapping siehe [`docker-compose.yaml`](../../docker-compose.yaml)). Das
Image laeuft im Produktionsmodus (`DJANGO_DEBUG=0`, gunicorn statt
`runserver`) und braucht `DJANGO_SECRET_KEY`; Compose setzt dafuer einen
Entwicklungswert.

## Code-Qualitaet

Wie im Backend (siehe [`radish/backend/README.md`](../backend/README.md),
Abschnitt "Code-Qualität") pruefen neben den Tests drei Werkzeuge den Code,
mit denselben Regeln. Sie stehen in `requirements-dev.txt`, das
`requirements.txt` einbindet; das Image installiert nur `requirements.txt`.

```bash
pip install -r requirements-dev.txt
python manage.py test instances
mypy config instances
pylint config instances
scripts/lizard.sh
```

Alle vier sollen ohne Befund durchgehen. Es gibt noch keine CI und keine
pre-commit-Hooks: die Pruefungen werden vor dem Commit von Hand ausgefuehrt.

### Typpruefung mit mypy

Konfiguration: [`mypy.ini`](mypy.ini), mit den Plugins von `django-stubs` und
`djangorestframework-stubs`. Geprueft wird gegen Python 3.12, die Version des
Images (`docker/game-server/Dockerfile`).

- **Strikt:** jede Funktion hat vollstaendige Type Hints fuer Parameter und
  Rueckgabe.
- **Lokale Variablen** sind ebenfalls annotiert; Variablen von Schleifen,
  `with` und Tuple-Zuweisungen werden dafuer in einer eigenen Zeile davor
  deklariert (`code: str` vor `for code in codes:`). Ausgenommen sind nur
  Comprehension-Variablen, `except ... as e` und Lambda-Parameter. mypy kann
  das nicht erzwingen -- es ist eine Konvention, die beim Review geprueft wird.
- **Kein `Any` ohne Grund:** strukturierte Daten bekommen einen `TypedDict`
  (etwa `GameStartFile` fuer `spielstart.json`), Pruefungen, die einen Typ
  sicherstellen, einen `TypeGuard` (`valid_access_codes`). `cast(...)` und
  `# type: ignore[...]` nur mit Kommentar, warum.

### Statische Analyse mit pylint

Konfiguration: [`.pylintrc`](.pylintrc), mit `pylint-django`.

- Zeilenlaenge hoechstens **120** Zeichen, Docstrings sind keine Pflicht.
- Abgeschaltet, weil es DRF-Muster sind: `abstract-method` und
  `too-many-ancestors`.
- Alles andere ist an. Ausnahmen an einzelnen Stellen stehen als
  `# pylint: disable-next=<meldung>` (bzw. `disable=` oben in der Datei) mit
  einer Begruendung in der Zeile darueber -- etwa fuer die Mock-Parameter der
  `@mock.patch`-Dekoratoren in `instances/tests.py`.

### Komplexitaet mit lizard

Aufruf und Grenzwerte: [`scripts/lizard.sh`](scripts/lizard.sh); lizard hat
keine Konfigurationsdatei. Je Funktion hoechstens **CCN 5**, **500 Zeilen** und
**6 Parameter**, keine Erweiterungen, Migrationen ausgenommen. Wird eine
Funktion zu komplex, wird sie in kleine Pruef- oder Hilfsfunktionen
aufgeteilt (z.B. `_start_request_error` mit der Tabelle `_START_CHECKS`), ohne
das Verhalten zu aendern.
