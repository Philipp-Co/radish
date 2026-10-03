# radish-frontend

Angular-Frontend fuer radish: Login (Keycloak, Authorization Code Flow mit
PKCE gegen den oeffentlichen Client `radish-web`), Uebersicht/Erstellen/
Beitreten offener Spiele (`api/client/game*`, siehe
`radish/backend/api/client_views.py`) und die eigentliche Spielansicht, die
den bestehenden WASM-Client (`radish/client/`, gebaut nach `radish/web/`)
in einem `<canvas>` einbindet. Die bisherige WebRTC/Relay-Verbindung zum
Spielserver ist entfernt (siehe Root-`README.md`, Abschnitt "Datenfluss")
-- eine Anbindung an den WebSocket des Backends
(`radish/backend/api/consumers.py`) ist noch offen.

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
  laufen, siehe Root-`README.md`.

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

Login: `test-user` / `test-user` bzw. `test-user-2` / `test-user-2` (beide
Rolle "Player" -- zwei Spieler, um Lobby und Spielstart durchzuspielen) oder
`radish-admin` /
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
  sieben Unterseiten als eigene Kind-Routen (`/games/current`, `/games/lobby`,
  `/games/list`, `/games/create`, `/games/join`, `/games/armies`,
  `/games/admin`):
  - `current/` -- eigenes laufendes Spiel, falls vorhanden (`api/client/game/current/`);
    bettet dafuer `shared/game-canvas/` direkt ein, ohne auf `/game/:name`
    umzuleiten. Steht das Spiel noch nicht auf `running`, leitet die Seite
    auf `lobby/` um; pollt alle 5s, ob der Gegner das Spiel beendet hat.
    Verlassen beendet das Spiel fuer beide Spieler.
  - `lobby/` -- Wartebereich nach Erstellen/Beitreten, pollt
    `api/client/game/current/` alle 2s. Der Host startet das Spiel per
    Knopf (`api/client/game/start/`), sobald ein Gegner da ist. Der Aufruf
    ist synchron: er kehrt erst zurueck, wenn die Spielinstanz das Spiel
    aufgesetzt hat, und liefert es dann als `running` -- dann geht es weiter
    auf `current/`. Der zweite Spieler erfaehrt den Start ueber das Polling.
  - `list/` -- offene Spiele (in der Lobby) samt Punktelimit, pollt alle 5s.
  - `create/` -- Spiel erstellen: Name, Passwort, Punktelimit und die eigene
    Armee, mit der man antritt (`army-picker.component.ts`; Armeen ohne
    Einheiten oder ueber dem Limit sind nicht waehlbar).
  - `join/` -- Spiel beitreten (Name vorbelegt, wenn man aus `list/` kommt),
    ebenfalls mit Armeeauswahl gegen das Punktelimit des Spiels.
  - `armies/` -- eigene Armeen (`api/client/armies/`): Liste mit Loeschen
    (`list/`), Anlegen und Bearbeiten unter `/games/armies/new` bzw.
    `/games/armies/:id` (`edit/`). Der Editor hat zwei Stufen auf einer
    Seite: eine Uebersicht mit einer Zeile je Einheit (Zusammensetzung,
    schwere Waffe, Kosten, Regelverstoesse) und die Detailansicht einer
    Einheit (`?unit=<Nr.>`, `unit-detail.component.ts`) mit einer Zeile je
    Entitaet. Er waehlt nur aus dem Katalog (`api/client/catalog/`) aus
    und rechnet die Kosten (Profil + Waffen + Ausruestung, auch die
    schweren Waffen der Entitaeten) live mit (`army-draft.ts`). Waffen haben
    eine Klasse (Standard, schwer, super-schwer); alle traegt eine einzelne
    Entitaet, in so vielen Slots je Klasse, wie ihr Entitaetsprofil
    vorsieht.
  - `admin/` -- nur sichtbar/erreichbar mit der Keycloak-Realm-Rolle
    "Radish-Admin", mit Unternavigation (`admin-shell.component.ts`):
    - `/games/admin/servers` -- listet die angemeldeten Game-Server auf
      (`api/admin/servers/`, pollt alle 5s wie `list/`).
    - `/games/admin/catalog` -- pflegt den Armee-Katalog
      (`api/admin/catalog/`, Modelle in `catalog/catalog-config.ts`): je
      Modell (Spezies, Einheitentypen, Entitaetsprofile, Waffen,
      Ausruestung) eine Seite zum Anlegen (`.../new`) und eine zum Aendern
      (`.../edit`) mit Suche im Namen -- bewusst keine Liste aller
      Eintraege. Alle Werte sind jederzeit aenderbar, auch wenn Spieler den
      Eintrag verwenden; nur Loeschen ist dann gesperrt.
- `src/app/shared/game-canvas` -- `GameCanvasComponent`: Canvas und
  WASM-Client (`client.js`/`client.wasm`), aktuell ohne Verbindung zu einem
  Spielserver (die bisherige WebRTC/Relay-Verbindung ist entfernt, siehe
  Root-`README.md`) -- eingebettet sowohl in `pages/game` (`/game/:name`)
  als auch direkt in `pages/games/current` (kein Seitenwechsel dafuer
  noetig).
- `src/app/pages/game` -- eigenstaendige Seite fuer `/game/:name`: nur noch
  Titel/Zurueck-Link als Rahmen um `GameCanvasComponent`. Nach dem
  Erstellen/Beitreten (`create/`/`join/`) geht es nicht mehr hierher,
  sondern zu `/games/lobby` -- die Route bleibt aber fuer einen direkten
  Aufruf bestehen.
