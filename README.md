# radish

Isometrischer Spielclient in C, der als WebAssembly im Browser läuft und über
den WebSocket des Django-Backends mit einem UDP-Spielserver sprechen soll.

Das Repository fasst mehrere Teile zusammen: den Client selbst (`radish/`) sowie
die Container-Definitionen, mit denen sich das Ganze lokal starten lässt
(`docker/`).

## Datenfluss

Der bisherige Relay, der einen WebRTC-DataChannel im Browser per UDP-Bruecke
mit dem Spielserver verband, ist entfernt (vormals `relay/`, `aiortc` +
`websockets`, samt der Testseite `radish/web/index.html`). Die UDP-Bruecke soll stattdessen ueber das Django-Backend
laufen: dessen WebSocket (`radish/backend/api/consumers.py`, `EchoConsumer`)
oeffnet serverseitig bereits ein UDP-Socket zum `GameServer` des aktuellen
Spiels und leitet Daten in beide Richtungen weiter. Der WASM-Client im
Browser ist an diesen WebSocket aber noch nicht angebunden (siehe
`radish/frontend/src/app/shared/game-canvas/game-canvas.component.ts`) --
das ist ein noch offener, spaeterer Schritt.

Rechts vom Browser/WASM-Client haengt der Server dieses Projekts. Aus Sicht
von Zucchini ist er ein lokaler Client: zwei Ringpuffer im Shared Memory
plus eine FIFO zum Aufwecken, gekapselt in der Zucchini-Api. Es sind also
zwei Prozesse — `zucchini_server` nimmt die UDP-Pakete an, `server` hält
den Spielzustand. Das 8-Byte-Codefeld vor jedem Paket wertet Zucchini selbst
aus (Whitelist) und schneidet es ab. Gesetzt wird es nicht vom Client, sondern
vom WebSocket-Consumer im Backend, zusammen mit der Spieler-Id dahinter
([access.py](radish/backend/api/access.py)):

```
[ Zucchini-Code 8 Byte ][ Spieler-Id 8 Byte ][ NetUserRequest vom Client ]
```

Der Code ist geheim — das Backend vergibt ihn je Spieler beim Spielstart, die
Instanz trägt ihn in die Whitelist ein, kein Client sieht ihn. Die Spieler-Id
ist öffentlich: die Kennung des Spielers, in eine `uint64_t` gepackt. Der
Spielserver liest den Absender aus ihr (`RAD_ParseSenderFromMessage`), und der
Client bekommt seine eigene beim Verbinden, um Eigenes von Fremdem zu
unterscheiden.

## Verzeichnisse

| Pfad | Inhalt |
|---|---|
| `radish/` | Das Spiel: CMake-Dachprojekt über drei Unterprojekte |
| `radish/game/` | Bibliothek `radish_game` — Spiellogik: Spiel, Welt, Tiles, Einheiten, dazu Kommandos und das Laden der Weltdefinition |
| `radish/client/` | Das Wasm-Programm: `main.c` und die isometrische Darstellung mit SDL2 |
| `radish/game-server-core/` | Das Host-Programm: `main.c`, hängt über die Zucchini-Api am Netz |
| `radish/game/test/` | Die Tests zu `radish_game`, ein Verzeichnis und ein Programm je Modul |
| `radish/cmake/` | CMake-Beiwerk: die Einbindung von `jsmn`, `zucchini` und `Unity` |
| `radish/web/` | Build-Ziel des Wasm-Clients: die daneben erzeugten `client.js` / `client.wasm` |
| `radish/frontend/` | Angular-Frontend: Login, Spielübersicht, Spielansicht, Adminbereich (Rolle "Radish-Admin", listet angemeldete Game-Server) — bindet `radish/web/` unter `/wasm/` ein — siehe [radish/frontend/README.md](radish/frontend/README.md) |
| `radish/backend/` | Django-Matchmaking-API + Keycloak-Login (`api/`), liefert `radish/frontend/` und `radish/web/` unter `/` bzw. `/wasm/` mit aus (`web/`) |
| `radish/assets/` | Schrift, wird per `--embed-file` ins Wasm-Modul eingebettet |
| `docker/` | Dockerfiles: Backend (Zucchini + Spielserver + Django, baut dabei `radish/frontend/` mit) |

Nicht im Repository, aber zum Bauen nötig (siehe unten): `emsdk/`, `jsmn/`,
`zucchini/`, `Unity/`.

## Aufbau

`radish/` ist in drei Unterprojekte geschnitten, die in genau eine Richtung
voneinander abhängen:

```
                    ┌──liest──  client
   radish_game  ◄───┤        (Wasm-Programm)
   (Bibliothek)     └──liest──  server
                            (Host-Programm)
```

Unten die eine statische Bibliothek, darüber die zwei ausführbaren Ziele. Damit
steht die Abhängigkeitsrichtung nicht mehr nur in dieser Datei, sondern im Build:
`radish_game` kennt das Rendering nicht und kann es auch nicht versehentlich
benutzen.

Das Einlesen der Weltdefinition liegt *in* `radish_game`
(`game/src/serialization/`). Der Grund ist die Kapselung des Modells: der Leser
schreibt in die Welt, braucht also ihre Struktur — und die steht hinter
`game/src/include/`. Eine Bibliothek daneben hätte diesen Pfad von außen
gebraucht, und damit wäre die Grenze für alle offen gewesen.

Client und Server schließen sich im Build aus, weil ihre Umgebungen es tun: der
Client hängt an Emscripten, der Server an POSIX (Shared Memory, `mkfifo`,
`poll`). Welches der beiden gebaut wird, entscheidet allein die Toolchain —
siehe [Bauen](#bauen).

**`game/`** ist die Spiellogik und hat **keine** Abhängigkeiten — kein SDL, kein
Emscripten, keinen Parser. Ein Spiel besteht aus einer Welt, eine Welt aus einem
2D-Raster von Tiles und einem Pool von Einheiten. Es gilt: *pro Tile steht zu
jedem Zeitpunkt höchstens eine Einheit.* Tile und Einheit kennen beide die
Position, geschrieben wird sie aber ausschließlich von `RAD_WorldSpawnUnit`,
`RAD_WorldDeployUnit`, `RAD_WorldMoveUnit` und `RAD_WorldRemoveUnit` —
so kann die Doppelbuchführung nicht auseinanderlaufen.
`RAD_WorldIsConsistent` prüft sie vollständig nach.

Einheiten werden über `RAD_UnitId_t` referenziert, den Slot-Index im Pool.
Anders als ein Zeiger kann er nicht baumeln, und er wird nie neu vergeben. Eine
Einheit durchläuft drei Zustände (`RAD_UnitState_t`): in der **Reserve** ist sie
dem Spiel bekannt, steht aber auf keinem Feld (`RAD_WorldAddReserveUnit`), auf dem
**Feld** steht sie nach `RAD_WorldDeployUnit`, und **zerstört** behält sie ihren
Slot. Die Werte aus der Armee — Einheitentyp, Bewegung, Mitglieder mit Profil und
Waffen — trägt sie in festen Feldern mit
([unit.h](radish/game/include/radish/game/model/unit/unit.h)).

In `game/src/control/command/` liegt daneben das Kommando: ein Anlass in
Datenform, die Absicht den Zustand zu ändern, ohne ihn schon zu ändern
([command.h](radish/game/include/radish/game/control/command/command.h)). Dazu
seine Übersetzung auf die Strecke — das Format steht geschlossen in
[codec.h](radish/game/include/radish/game/control/command/codec.h), je eine Datei
beschreibt die Nutzlast einer Kommandoart, und `byte_writer`/`byte_reader` nehmen
ihnen die Byte-Reihenfolge ab: eine Datei beschreibt Felder, nicht Bytes.

Der Kopf jedes Kommandos trägt neben Art und Sequenznummer den **Absender**:
`RAD_UserId_t` aus [user.h](radish/game/include/radish/game/user.h), die Uuid des
Benutzers als `uint64_t`. Er steht im Kopf, weil jedes Kommando einen hat und
weil der Server ihn sonst nicht erfahren könnte — das Codefeld, mit dem Zucchini
den Rückweg kennt, ist abgeschnitten, bevor die Nutzlast ankommt. Die
Sequenznummer zählt damit je Benutzer, nicht über alle zusammen. Der Typ liegt im
Spielmodul, weil der Codec ihn schreibt und liest; wer mitspielt, weiß trotzdem
nur der Server (siehe unten) — eine Welt kennt Einheiten, keine Konten.

Der Rückweg liegt daneben in
[response.h](radish/game/include/radish/game/control/command/response.h) — die
Antwort trägt Art und Sequenznummer ihres Kommandos zurück, und nur daran erkennt
der Absender, worauf sie antwortet. Die neun Byte Kopf schreibt deshalb für
Kommando und Antwort dieselbe Funktion; liefen sie auseinander, wäre die
Zuordnung hin.

Das kostet `game/` keine Abhängigkeit — der Codec braucht nichts als `stdint.h`
und die Typen, die er schreibt. Deshalb ist es auch eine Bibliothek und nicht
zwei: dass `control/` früher ein eigenes Ziel war, lag allein am Loader darin, der
die Serialisierung brauchte.

**`client/`** ist das Programm: `main.c` und darunter `src/rendering/`, das mit
SDL2 auf ein Canvas zeichnet. Die Typen tragen dort das Präfix `RAD_Iso*`
(`RAD_IsoMap_t`, `RAD_IsoObject_t`) und sind bewusst von den Spiel-Typen
getrennt — eine `RAD_Unit_t` aus `game/` ist etwas anderes als das, was am
Bildschirm erscheint. Das Rendering bleibt Teil des Clients und keine eigene
Bibliothek: es hängt wie er an SDL und hat genau einen Nutzer.

**`game-server-core/`** ist die Gegenseite: dasselbe `radish_game`, aber statt SDL die
Zucchini-Api. Er hält den Spielzustand und wartet in einer Schleife auf
Nachrichten (`ZUC_ApiReceive`, dann `ZUC_ApiWait` auf der Wakeup-FIFO). Ein
Wakeup heißt „es liegt etwas an", nicht „genau eine Nachricht" — deshalb wird
der Ringpuffer erst leergeräumt und dann gewartet. Der Name der Zucchini-Instanz
ist standardmäßig `zucchini`, wie in `zucchini_server` selbst; ein anderer geht
als erstes Argument.

Darunter liegt `src/interface/` — die Außengrenze des Servers, in der aus einer
Nachricht ein Kommando wird und aus einer Antwort wieder eine Nachricht
([command.h](radish/game-server-core/src/include/radish/server/interface/command.h)). Das Modul kennt Zucchini
nicht: es bekommt einen Bytebereich und schreibt in einen, ist also ohne Shared
Memory prüfbar. Und es ändert nie einen Spielzustand. Das Byteformat selbst liegt
nicht hier, sondern beim Kommando (siehe `game/`); dieses Modul ist der Adapter
darauf und dünn mit Absicht — ihm bleiben die Fragen der Nachricht: wo die Bytes
anfangen und aufhören, wie viel Platz die Antwort hat, und dass nichts
Abgeschnittenes hinausgeht. Ein eigenes Ergebnis-Enum hat es nicht: das des
Codecs geht durch, denn ein zweites daneben wäre entweder ungenauer oder eine
Kopie. Eingebunden wird es beim Namen seines Moduls,
`#include <radish/server/interface/command.h>` — wie bei den Bibliotheken.

Daneben liegt `src/control/` — was mit einem Kommando geschieht
([execute.h](radish/game-server-core/src/include/radish/server/control/execute.h)). Die
Grenze zu `interface/` ist scharf: dort geht es um Bytes, hier um Bedeutung.
`RAD_ControlExecuteCommand` bekommt das Kommando **const** — es ist der Anlass,
nicht der Zustand — prüft es und gibt die fertige Antwort zurück, mit dem
Ergebnis in `value` und dem Kommando als genauer Kopie. Es ist die einzige
Stelle, die Teilnehmerliste und Spielzustand zusammen sieht, und sie muss es: ob
ein Kommando gilt, hängt an beidem. Geprüft wird, ob es einen Absender hat, ob
der mitspielt und ob die Figur, die es anfasst, nicht einem anderen gehört.

Zwei Schritte, in dieser Reihenfolge: **erst darf-er-das, dann geht-das.** Steht
das Erste fest, übernimmt je eine Datei unter `src/control/execute/` die
Ausführung ihrer Kommandoart — bisher nur
[move.c](radish/game-server-core/src/control/execute/move.c), der Rest liefert
`RAD_CONTROL_ERROR_NOT_EXECUTED`. Diese Ausführenden sind wie der Roster privat:
ihr Header liegt neben der Quelle, denn sie prüfen nichts mehr — wäre einer von
außen erreichbar, ließe sich ein Zug an der Berechtigung vorbei ausführen.
Gezogen wird über `RAD_WorldMoveUnit`, die einzige Stelle, die Tile und Einheit
synchron hält; ein abgelehnter Zug lässt die Welt garantiert unverändert, und
`value` sagt warum (`TARGET_OCCUPIED`, `OUT_OF_BOUNDS`, `NO_SUCH_ENTITY`).

Daneben liegt in `control/` der Loader
([loader.h](radish/game-server-core/src/include/radish/server/control/loader.h)) — woher
das Spiel kommt, an dem der Server arbeitet. Dieselbe Frage von der anderen
Seite: `execute` entscheidet, was mit dem Spielzustand geschieht, der Loader
bringt ihn hervor. `main` holt das Spiel dort und füttert damit die Steuerung,
kennt seine Herkunft also nicht. `RAD_ControlCreateGame(world_path)` legt ein
leeres Spiel an und lädt, wenn ein Pfad dasteht, eine Weltdefinition (Gelände,
Höhen, Größe — Format in
[world.schema.json](radish/game/schema/world.schema.json)); ohne Pfad bleibt es
leer, mit einem Pfad, der nicht trägt, gibt es kein Spiel und der Server bricht
ab. Der Event-Manager, an dem das Spiel hängt, gehört dabei dem
Loader — er liegt auf dem Heap und wird in `RAD_ControlDestroyGame` mit
abgebaut, sodass der Aufrufer nichts länger am Leben halten muss als das Spiel
selbst.

Die Armeen kommen später, mit dem Spielstart: das Backend legt die Datei ab
([spielstart.schema.json](radish/game/schema/spielstart.schema.json)) und meldet
sich per `SIGUSR1`. Der Server liest sie vollständig ein
([game_start.h](radish/game-server-core/src/include/radish/server/control/game_start.h))
und richtet das Spiel mit `RAD_ControlStartGame` ein — genau einmal: beide
Spieler spielen mit, der Host zuerst, und ihre Einheiten stehen in der Reserve.
Die Id eines Spielers im Spiel ist seine Kennung, Zeichen für Zeichen in die acht
Bytes einer `uint64_t` gepackt (`RAD_ControlUserIdFromIdentifier`).

Die Datei selbst liest das Spielmodul: `RAD_LoadWorldFromFile` aus
[game.h](radish/game/include/radish/game/game.h) macht sie auf, misst sie, liest
sie am Stück und prüft den Inhalt — das Format steht in
`game/src/serialization/` und nur dort. Entschieden wird dort nichts: ob ein
misslungener Ladevorgang den Server anhält, steht in `loader.c`.

Der Zustand von `execute` ist ein unvollständiger Typ, `RAD_Control_t`, wie
`ZUC_Api_t` — angelegt mit `RAD_CreateControl`, abgebaut mit
`RAD_DestroyControl`. Darin liegt die Teilnehmerliste: `src/control/session/`,
eine Tabelle fester Größe, die zur Uuid aus dem Kommandokopf den Mitspieler und
dessen Figuren findet — und umgekehrt zur Figur ihren Besitzer. Ein Benutzer
führt beliebig viele Figuren, eine Figur gehört höchstens einem Benutzer; die
zweite Hälfte ist die wichtige, denn nur durch sie ist „darf der das bewegen?"
überhaupt entscheidbar. Die Liste je Mitspieler ist so lang wie der
Einheitenpool der Welt und kann deshalb nie voll laufen — im Grenzfall gehören
alle Figuren demselben. Ihr Header liegt
als einziger im Server **nicht** unter `src/include/`, sondern neben seiner
Quelle: der Roster ist ein Modul im Innern von `control/`. Wer einen Benutzer
anlegen oder ihm eine Figur zuordnen will, geht durch `RAD_ControlAddUser` und
`RAD_ControlBindUserUnit` — sonst ließe sich an der Prüfung vorbei ändern, wer
mitspielt und wem was gehört. Nachgeschlagen wird linear; bei acht Plätzen wäre
jede Beschleunigung teurer als die Suche. Und wie `interface/` ändert der Roster
**nie** einen Spielzustand: eine Figur entsteht im Spielmodul, hier wird nur
vermerkt, wem sie gehört. Im Server steht er und nicht in `radish_game`, weil ein
Benutzer existiert, weil eine Verbindung existiert — und davon weiß eine Welt
nichts.

**`game/src/serialization/`** liest die Weltdefinition aus JSON. `json_reader`
kapselt den Token-Lauf über jsmn, `world_definition` baut daraus die Welt auf,
und `world_file` liest die Datei ein. Nach außen geht davon nur
`RAD_LoadWorldFromFile` in
[game.h](radish/game/include/radish/game/game.h).

Weil `game/` kein SDL zieht, lässt sich die Bibliothek ohne Emscripten auf dem
Host bauen — siehe [Bauen](#bauen).

Die Bibliothek legt ihre öffentlichen Header unter
`<projekt>/include/radish/<modul>/` ab und gibt dieses Verzeichnis `PUBLIC`
weiter. Deshalb bindet man sie überall gleich ein, egal von wo:

```c
#include <radish/game/game.h>
```

Client und Server halten es genauso, nur liegt ihr `include/` innerhalb von
`src/` und geht `PRIVATE` statt `PUBLIC` — sie sind Programme, aus denen niemand
etwas einbindet:

```c
#include <radish/rendering/iso_map.h>          // client/src/include/
#include <radish/server/interface/command.h>   // game-server-core/src/include/
```

> Stand jetzt ruft [main.c](radish/client/src/main.c) noch ausschließlich das
> Rendering auf. Das Game-Modul ist gebaut und getestet, aber noch nicht
> angebunden.
>
> Der Server liest eingehende Nachrichten als Kommando, gibt sie an `control/` und
> schickt die Antwort zurück — `handle_message` in
> [main.c](radish/game-server-core/src/main.c). **Ausgeführt** wird davon bisher
> `move_unit`; die übrigen vier Arten werden geprüft und mit
> `RAD_CONTROL_ERROR_NOT_EXECUTED` beantwortet, ihr Ausführender fehlt noch.
>
> Der Spielzustand dazu kommt aus dem Loader: die Welt aus der Weltdefinition,
> auf die `RADISH_WORLD_PATH` zeigt — das Docker-Image bringt
> [assets/worlds/](radish/assets/worlds/) unter `/usr/local/share/radish/worlds/`
> mit und setzt die Variable auf `default.json` —, ohne sie 8×8 Grund.
> Zugeordnet wird eine Figur bisher von niemandem
> (`RAD_ControlBindUserUnit` hat keinen Aufrufer): solange keine einen Besitzer
> hat, darf jeder Mitspieler jede ziehen.
>
> Fortgeschrieben wird schon die Teilnehmerliste: wer sendet, spielt mit. Ein
> Beitritt ist im Protokoll nicht vorgesehen, und eine getrennte Verbindung meldet
> Zucchini dem Server auch nicht — deshalb ruft `handle_message` für jedes
> eingehende Kommando `RAD_ControlAddUser`, und `RAD_ControlRemoveUser` hat noch
> keinen Aufrufer. `RAD_ControlBindUserUnit` auch nicht: eine Einheit bekommt
> ihren Besitzer mit dem Spielstart (`RAD_ControlStartGame`) und kommt mit
> `deploy_unit` aus der Reserve aufs Feld. Seine eigene Spieler-Id bekommt der
> Client vom Backend beim Verbinden (`RAD_ClientSetPlayerId` in
> [main.c](radish/client/src/main.c)).
>
> Auf eine Nachricht, die kein Kommando ist, geht nichts zurück: eine Antwort
> trägt den Kopf ihres Kommandos, und den gibt es dann nicht. Was ein Absender
> stattdessen erfahren sollte, ist eine offene Frage des Protokolls.

## Bauen

Der Client wird mit der Emscripten-Toolchain übersetzt:

```bash
source emsdk/emsdk_env.sh && emcmake cmake -S radish -B radish/build && cmake --build radish/build
```

Das Ergebnis landet als `client.js` und `client.wasm` direkt in `radish/web/`,
neben der `index.html`. Zusätzlich kopiert der Build die
`compile_commands.json` nach `radish/`, damit clangd im Editor die
SDL2-Header findet.

Ohne Emscripten baut dasselbe Projekt die Bibliotheken und den Server; der
Client wird übersprungen statt den Build abzubrechen:

```bash
cmake -S radish -B build-host && cmake --build build-host
```

Das Ergebnis liegt als `build-host/game-server-core/server` im Build-Verzeichnis. Zu
diesem Build gehören auch `zucchini_utils` und `zucchini_api` — sie werden aus
dem Klon neben dem Projekt mitübersetzt, siehe
[Externe Abhängigkeiten](#externe-abhängigkeiten). Fehlt der Klon, bricht CMake
mit einer entsprechenden Meldung ab.

Dieser Build kopiert die `compile_commands.json` bewusst *nicht* nach `radish/` —
sonst stünden dort Einträge ohne die SDL2- und Emscripten-Suchpfade, und clangd
fände die Header des Clients nicht mehr.

SDL2 und SDL_ttf werden nicht separat installiert — Emscripten zieht beide über
`-sUSE_SDL=2` / `-sUSE_SDL_TTF=2` als Ports.

## Testen

Die Tests gehören zum Host-Build: es sind Programme, die auf der Maschine laufen
und deren Rückgabewert CTest liest. Unter Emscripten werden sie nicht gebaut —
dort gäbe es nichts auszuführen.

```bash
cmake -S radish -B build-host && cmake --build build-host
ctest --test-dir build-host
```

Fünf Programme, eines je Modul unter `radish/game/src/`:

| Test | prüft |
|---|---|
| `game` | die Fassade: Mitspieler, Zug, Besitz (`game.c`, `player.c`) |
| `model` | Welt und Zug — den Spielzustand selbst |
| `control_command` | den Codec und die Nutzlast je Kommandoart |
| `control_events` | den Event-Manager: abonnieren und veröffentlichen |
| `control_execute` | die Kommando-Fabriken |

Schlägt etwas fehl, zeigt CTest von sich aus nur den Namen; die Ausgabe von Unity
bekommt man mit:

```bash
ctest --test-dir build-host --output-on-failure
```

Ein einzelner Test läuft über seinen Namen — `-R` ist ein regulärer Ausdruck:

```bash
ctest --test-dir build-host -R model
```

Ein Testprogramm lässt sich auch direkt starten, dann steht jede Testfunktion mit
Datei und Zeile da:

```bash
./build-host/game/test/model/test_model
```

Solange die Umstellung des Modells auf private Header nicht bis in die
Serialisierung nachgezogen ist, bricht `cmake --build build-host` vorher ab. Die
Testziele hängen nicht daran und lassen sich einzeln bauen:

```bash
cmake --build build-host --target test_game test_model test_control_command test_control_events test_control_execute
```

### Aufbau

`radish/game/test/` spiegelt `radish/game/src/`: zu jedem Modulverzeichnis dort
gehört eines hier, und darin entsteht genau ein Programm. Wer eine Quelle ändert,
weiß ohne Suchen, wo ihre Tests stehen.

Die Tests bekommen den privaten Suchpfad des Moduls (`radish/game/src/include/`)
ausdrücklich mit. Das hebt die Grenze nicht auf, sondern zieht sie: ein Test ist
kein Aufrufer von außen, er gehört zum Modul — und ohne den Pfad ließe sich die
halbe Spiellogik nicht prüfen, weil `RAD_Turn_t` und `RAD_World_t` unvollständig
blieben.

Als Testrahmen dient [Unity](https://github.com/ThrowTheSwitch/Unity), ohne
seinen Ruby-Generator: die `main.c` je Verzeichnis zählt die Testfunktionen von
Hand auf, und `setUp`/`tearDown` stehen dort, weil Unity sie je Programm genau
einmal erwartet. Ein neues Testverzeichnis ist eine Zeile in
[radish/game/test/CMakeLists.txt](radish/game/test/CMakeLists.txt):

```cmake
rad_add_game_test(model main.c test_turn.c test_world.c)
```

## Starten

```bash
docker compose up --build
```

Danach ist alles unter <http://localhost:8000> erreichbar: Login,
Spielübersicht und Spielansicht (`radish/frontend/`) ebenso wie der
Wasm-Client (`radish/web/`, unter `/wasm/`) werden direkt vom
Django-Backend mit ausgeliefert (siehe `radish/backend/web/`) — kein
eigener nginx-Container mehr davor.

| Dienst | Port | Aufgabe |
|---|---|---|
| `backend` | 8000 → 8000 | Django: liefert `radish/frontend/` unter `/`, `radish/web/` unter `/wasm/` und die Matchmaking-/Login-API unter `/api/` |
| `game-server` | 9999/udp | Zucchini an der UDP-Strecke und der Spielserver daran |

Der Wasm-Client muss vor `docker compose up --build backend` gebaut sein —
das `backend`-Image kopiert `radish/web/` beim Bauen hinein.
`radish/frontend/` baut das Image dagegen in einem eigenen Node-Stage
selbst mit (siehe [radish/frontend/README.md](radish/frontend/README.md)),
dafür ist kein vorheriges `npm install` auf dem Host nötig.

### Das Backend-Image

`backend` ist der einzige Dienst mit zwei Prozessen, und das aus einem Grund:
`zucchini_server` und der Spielserver reden über Shared Memory und eine FIFO,
nicht über das Netz — getrennte Container gäbe das nicht her. Der
[Einstiegspunkt](docker/game-server/game-instance-entrypoint.sh) startet Zucchini
mit leerer Whitelist und dann den Spielserver. Die Codes der Spieler trägt erst das
Django der Instanz beim Spielstart über den Admin-Client ein und beim Abbruch
wieder aus ([views.py](radish/game-server/instances/views.py)). Ohne Eintrag
verwirft Zucchini jedes Paket, denn mit dem Code merkt es sich auch, wohin die
Antwort geht.

Gebaut wird [aus dem Quelltext](docker/backend/Dockerfile), mit der Wurzel des
Repositorys als Kontext: das Image braucht `radish/`, `jsmn/` und `zucchini/`
zusammen. Was nicht in den Kontext gehört, steht in
[.dockerignore](.dockerignore) — ohne die gingen die 2,1 GB aus `emsdk/` mit,
auch bei `web`.

Nur das Backend, ohne Browser:

```bash
docker compose up --build backend
```

Zum Prüfen braucht es ein UDP-Paket aus acht Byte Code (big endian, einer aus der
Whitelist), acht Byte Spieler-Id und einer `NetUserRequest` dahinter ([message.proto](radish/protobuf/message.proto)) — etwa
ein `NetDeployCommand` ([command.proto](radish/protobuf/command.proto)), das eine
Einheit aus der Reserve aufstellt. Zurück kommt eine `NetCommandResponse` mit dem
Kommando, `success` und dem Grund als Text. Alles, was kein Kommando ist, wird
verworfen und nur geloggt.

## Externe Abhängigkeiten

Drei Verzeichnisse gehören nicht ins Repository und sind in der
[.gitignore](.gitignore) ausgenommen; sie müssen vor dem ersten Bauen daneben
liegen:

| Verzeichnis | Was | Woher |
|---|---|---|
| `emsdk/` | Emscripten-SDK für den Wasm-Build | <https://github.com/emscripten-core/emsdk> |
| `jsmn/` | JSON-Tokenizer, ein Header, MIT | <https://github.com/zserge/jsmn> |
| `zucchini/` | UDP-Backend, eigenes Projekt | separat auschecken |
| `Unity/` | Testrahmen für die Unit-Tests, MIT | <https://github.com/ThrowTheSwitch/Unity> |

`jsmn` wird nicht kopiert, sondern aus dem Klon eingebunden — die Suche und das
zugehörige CMake-Target stehen in [radish/cmake/jsmn.cmake](radish/cmake/jsmn.cmake).
Fehlt der Klon, bricht CMake mit einer entsprechenden Meldung ab, statt später
über einen fehlenden Header zu stolpern. Wer ihn woanders liegen hat, setzt
`-DJSMN_INCLUDE_DIR=<pfad>`.

`zucchini` genauso, seit es den Server gibt: es ist nicht mehr nur zur Laufzeit
nötig, sondern beim Bauen. Aus dem Klon werden zwei Bibliotheken mitübersetzt,
`zucchini_utils` und `zucchini_api` — nicht sein Dachprojekt, das würde Server,
Admin-Client und Testwerkzeuge mitziehen. Das steht in
[radish/cmake/zucchini.cmake](radish/cmake/zucchini.cmake); ein anderer Ort geht
über `-DZUCCHINI_DIR=<pfad>`. Eingebunden wird die Datei nur im Host-Build: unter
Emscripten ließe sich `zucchini_api` nicht übersetzen, und ein Wasm-Build soll
nicht an einem fehlenden `zucchini/` scheitern.

`Unity` ebenso, und aus demselben Grund nur im Host-Build. Aus dem Klon wird eine
einzige Übersetzungseinheit mitgebaut (`src/unity.c`) — nicht sein Dachprojekt,
das brächte Versionsableitung und Install-Regeln mit, die dieser Build nicht
braucht. Das steht in [radish/cmake/unity.cmake](radish/cmake/unity.cmake); ein
anderer Ort geht über `-DUNITY_DIR=<pfad>`. Beachte das große U im
Verzeichnisnamen.
