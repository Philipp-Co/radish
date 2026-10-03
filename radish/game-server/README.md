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
[`instances/views.py`](instances/views.py). `<name>` ist der Name, unter dem
sich die Instanz beim Backend meldet (`RADISH_GAME_SERVER_NAME`, z.B.
`game-instance-1`).

| Methode | Pfad                                  | View             | Antwort |
|---------|---------------------------------------|------------------|---------|
| POST    | `/api/instances/<name>/game/start/`   | `GameStartView`  | 200 + `{"instance": ..., "status": "ready"}`, 400 ohne zwei Spieler unter `spieldaten`, 500 wenn die Datei nicht zu schreiben ist oder kein `radish_server` laeuft |
| POST    | `/api/instances/<name>/game/cancel/`  | `GameCancelView` | 204, loescht die Spielstart-Datei und benachrichtigt `radish_server` |

Ohne Authentifizierung und ohne CSRF-Pruefung -- nur fuer Dev, die Auth
kommt in `InstanceAPIView` hinzu.

Der Container selbst faehrt aber schon Django und eine Game-Instanz
nebeneinander und haelt beide am Leben: [`supervisord`](../../docker/game-server/supervisord.conf)
uebernimmt die Rolle von "etwas wie systemd" -- stuerzt Django oder das Paar
`zucchini_server`/`radish_server` ab, startet supervisord neu, was abgestuerzt
ist (siehe [`game-instance-entrypoint.sh`](../../docker/game-server/game-instance-entrypoint.sh)
fuer die Prozesspaarung der beiden C-Programme).

Vorlaeufig nur eine Instanz, nicht vier: `zucchini_server` hat Instanzname
und UDP-Port fest einkompiliert (siehe Kommentar in `supervisord.conf`) und
kann deshalb noch nicht mehrfach im selben Container laufen. Sobald das
behoben ist, kommen weitere `[program:game-instance-N]`-Sektionen dazu.

## Lokal starten

```bash
cd radish/game-server
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python manage.py migrate
python manage.py runserver
```

Ein Admin-Login braucht einen Superuser:

```bash
python manage.py createsuperuser
```

Danach ist die (bis auf `django.contrib.admin` selbst leere) Admin-Oberflaeche
unter <http://localhost:8000/admin/> erreichbar.

Tests:

```bash
python manage.py test instances
```

## Ueber Docker

```bash
docker compose up --build game-server
```

Danach unter <http://localhost:8090/admin/> erreichbar (Port-Mapping siehe
[`docker-compose.yaml`](../../docker-compose.yaml)).
