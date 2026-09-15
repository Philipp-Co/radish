# radish-frontend

Angular-Frontend fuer radish: Login (Keycloak, Authorization Code Flow mit
PKCE gegen den oeffentlichen Client `radish-web`), Uebersicht/Erstellen/
Beitreten offener Spiele (`api/client/game*`, siehe
`radish/backend/api/client_views.py`) und die eigentliche Spielansicht, die
den bestehenden WASM-Client (`radish/client/`, gebaut nach `radish/web/`)
in einem `<canvas>` einbindet und wie bisher per WebRTC/Relay mit dem
Backend spricht (siehe `radish/relay/server.py`).

Ersetzt das bisherige, sehr schlichte `radish/web/index.html` als
UI-Schicht -- `radish/web/` bleibt als Build-Ziel des WASM-Clients bestehen
(`client.js`/`client.wasm`, siehe `radish/client/CMakeLists.txt`,
`RUNTIME_OUTPUT_DIRECTORY`), wird aber nicht mehr direkt unter "/"
ausgeliefert, sondern unter "/wasm/" eingebunden, waehrend "/" jetzt dieser
Angular-App gehoert.

Es gibt dafuer keinen eigenen Webserver-Container: alles laeuft ueber das
Django-Backend (`radish/backend/`, Service `backend` in
`docker-compose.yaml`) -- Angular-Bundle unter `/`, `radish/web/` unter
`/wasm/`, die Matchmaking-/Login-API unter `/api/` (siehe
`radish/backend/web/`).

## Voraussetzungen

- Node.js (LTS) + npm
- Das uebrige `docker compose up` (Backend, Keycloak, ggf. game-server) muss
  laufen, siehe Root-`README.md`. Der `relay`-Server (`radish/relay/`) muss
  zusaetzlich separat gestartet werden -- er ist noch kein Teil von
  `docker-compose.yaml`.

## Lokale Entwicklung

```bash
cd radish/frontend
npm install
npm start
```

`npm start` (= `ng serve`) startet den Dev-Server auf `http://localhost:4200`
und nutzt `proxy.conf.json`, um `/api` und `/wasm` an das Django-Backend
(`http://localhost:8000`) weiterzureichen -- dadurch sind eigene
API-/Wasm-Aufrufe immer same-origin, ganz ohne CORS-Sonderfall fuer die
eigene API. Nur Keycloak (Port 8081) wird direkt vom Browser aus
angesprochen (Redirect-Flow + Token-Endpunkt), das ist bei einem
oeffentlichen PKCE-Client so vorgesehen und wird von Keycloak selbst per
`webOrigins` erlaubt (siehe `docker/keycloak/realm-radish.json`, Client
`radish-web` -- `http://localhost:4200` ist dort mit eingetragen).

Login: `test-user` / `test-user` (Rolle "Player") oder `radish-admin` /
`radish-admin` (Rolle "Radish-Admin", schaltet zusaetzlich den
Admin-Tab frei -- siehe realm-radish.json).

## Produktivbuild

```bash
npm run build
```

Baut nach `dist/radish-frontend/browser/`. `docker/backend/Dockerfile`
erledigt das automatisch als Teil von `docker compose up --build backend`
(ein eigener Node-Stage baut das Frontend, das Python-Image kopiert das
Ergebnis mit hinein -- siehe dort und `radish/backend/web/views.py`).

## Struktur

- `src/app/core/` -- `AuthService` (PKCE-Flow + Token-Refresh, liest
  ausserdem die Keycloak-Realm-Rollen aus dem Access-Token fuer
  `isAdmin()`), `authGuard`, `adminGuard` (zusaetzlich zu `authGuard`, nur
  mit Rolle "Radish-Admin"), `authInterceptor` (haengt den Bearer-Token an
  `/api/`-Aufrufe), `ApiService` (Matchmaking-Endpunkte, `listServers()` fuer
  den Adminbereich).
- `src/app/pages/login` -- Login-Seite.
- `src/app/pages/callback` -- Rueckweg von Keycloak, tauscht den Code gegen
  Tokens.
- `src/app/pages/games` -- Rahmen mit Menue (`games-shell.component.ts`) und
  fuenf Unterseiten als eigene Kind-Routen (`/games/current`, `/games/list`,
  `/games/create`, `/games/join`, `/games/admin`):
  - `current/` -- eigenes laufendes Spiel, falls vorhanden (`api/client/game/current/`);
    bettet dafuer `shared/game-canvas/` direkt ein, ohne auf `/game/:name`
    umzuleiten.
  - `list/` -- laufende Spiele, pollt alle 5s.
  - `create/` -- Spiel erstellen.
  - `join/` -- Spiel beitreten (Name vorbelegt, wenn man aus `list/` kommt).
  - `admin/` -- nur sichtbar/erreichbar mit der Keycloak-Realm-Rolle
    "Radish-Admin": listet die angemeldeten Game-Server auf
    (`api/admin/servers/`, pollt alle 5s wie `list/`).
- `src/app/shared/game-canvas` -- `GameCanvasComponent`: Canvas, WASM-Client
  (`client.js`/`client.wasm`) und WebRTC-Verbindung zum Relay -- eingebettet
  sowohl in `pages/game` (`/game/:name`) als auch direkt in
  `pages/games/current` (kein Seitenwechsel dafuer noetig).
- `src/app/pages/game` -- eigenstaendige Seite fuer `/game/:name`: nur noch
  Titel/Zurueck-Link als Rahmen um `GameCanvasComponent`. Nach dem
  Erstellen/Beitreten (`create/`/`join/`) geht es nicht mehr hierher,
  sondern zu `/games/current` -- die Route bleibt aber fuer einen direkten
  Aufruf bestehen.
