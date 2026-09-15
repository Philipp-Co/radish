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

Django selbst ist weiterhin eine leere Huelle: `django-admin startproject` +
eine leere App `instances`, kein Modell, keine View -- die eigentliche
Instanzverwaltung *durch* Django (anlegen, starten, stoppen ueber die
Admin-Oberflaeche) kommt in einem spaeteren Schritt.

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

## Ueber Docker

```bash
docker compose up --build game-server
```

Danach unter <http://localhost:8090/admin/> erreichbar (Port-Mapping siehe
[`docker-compose.yaml`](../../docker-compose.yaml)).
