# radish

Isometrischer Spielclient in C, der als WebAssembly im Browser läuft und über
eine WebRTC-Strecke mit einem UDP-Backend spricht.

Das Repository fasst mehrere Teile zusammen: den Client selbst (`radish/`), einen
Relay, der die Brücke zwischen Browser und UDP schlägt (`relay/`), sowie die
Container-Definitionen, mit denen sich das Ganze lokal starten lässt (`docker/`).

## Datenfluss

Der Browser kann kein UDP. Deshalb hängt zwischen Client und Backend ein Relay,
das einen WebRTC-DataChannel auf UDP-Pakete abbildet:

```
Browser                        Relay                     Zucchini                 radish
┌─────────────────┐            ┌──────────────┐          ┌──────────┐          ┌────────────┐
│ index.html      │  WebSocket │              │          │          │          │            │
│  client.js      │ ─────────► │  Signaling   │          │          │          │  Spiel-    │
│  client.wasm    │  :8765     │              │          │          │Ringpuffer│  zustand   │
│                 │            │              │          │          │◄────────►│            │
│  SDL2 / Canvas  │  DataChan. │  UdpBridge   │   UDP    │  Server  │  (SHM)   │  server    │
│                 │ ◄────────► │              │ ◄──────► │  :9999   │          │            │
└─────────────────┘   WebRTC   └──────────────┘          └──────────┘          └────────────┘
```

Rechts hängt der Server dieses Projekts. Aus Sicht von Zucchini ist er ein
lokaler Client: zwei Ringpuffer im Shared Memory plus eine FIFO zum Aufwecken,
gekapselt in der Zucchini-Api. Es sind also zwei Prozesse — `zucchini_server`
nimmt die UDP-Pakete an, `server` hält den Spielzustand. Das 8-Byte-Codefeld,
das der Client jedem Paket voranstellt, wertet Zucchini selbst aus (Whitelist)
und schneidet es ab; beim Server kommt nur die Nutzlast an.

Der Container hat keine vom Browser aus erreichbare eigene IP. Der Relay schränkt
die ICE-Portvergabe deshalb auf einen schmalen, 1:1 gemappten Bereich ein
(40000–40019/udp) und schreibt die Kandidaten-IP im SDP-Answer auf
`--external-ip` um. Details stehen in [docker-compose.yaml](docker-compose.yaml).

## Verzeichnisse

| Pfad | Inhalt |
|---|---|
| `radish/` | Das Spiel: CMake-Dachprojekt über vier Unterprojekte |
| `radish/game/` | Bibliothek `radish_game` — Spiellogik: Spiel, Welt, Tiles, Entitäten |
| `radish/serialization/` | Bibliothek `radish_serialization` — Speichern und Laden als JSON |
| `radish/client/` | Das Wasm-Programm: `main.c` und die isometrische Darstellung mit SDL2 |
| `radish/server/` | Das Host-Programm: `main.c`, hängt über die Zucchini-Api am Netz |
| `radish/game/test/` | Die Tests zu `radish_game`, ein Verzeichnis und ein Programm je Modul |
| `radish/cmake/` | CMake-Beiwerk: die Einbindung von `jsmn`, `zucchini` und `Unity` |
| `radish/web/` | `index.html` und die daneben erzeugten `client.js` / `client.wasm` |
| `radish/assets/` | Schrift, wird per `--embed-file` ins Wasm-Modul eingebettet |
| `relay/` | WebRTC-zu-UDP-Relay (Python, `aiortc` + `websockets`) |
| `docker/` | Dockerfiles: Backend (Zucchini + Spielserver), Relay, nginx für `radish/web/` |

Nicht im Repository, aber zum Bauen nötig (siehe unten): `emsdk/`, `jsmn/`,
`zucchini/`, `Unity/`.

## Aufbau

`radish/` ist in vier Unterprojekte geschnitten, die in genau eine Richtung
voneinander abhängen:

```
                                                   ┌──liest──  client
radish_serialization  ──liest──►  radish_game  ◄───┤        (Wasm-Programm)
        (Bibliothek)              (Bibliothek)     └──liest──  server
                                                            (Host-Programm)
```

Die beiden unteren Ebenen sind eigene statische Bibliotheken, darüber liegen die
zwei ausführbaren Ziele. Damit steht die Abhängigkeitsrichtung nicht mehr nur
in dieser Datei, sondern im Build: `radish_game` kennt weder die Serialisierung
noch das Rendering und kann sie auch nicht versehentlich benutzen.

Client und Server schließen sich im Build aus, weil ihre Umgebungen es tun: der
Client hängt an Emscripten, der Server an POSIX (Shared Memory, `mkfifo`,
`poll`). Welches der beiden gebaut wird, entscheidet allein die Toolchain —
siehe [Bauen](#bauen).

**`game/`** ist die Spiellogik und hat **keine** Abhängigkeiten — kein SDL, kein
Emscripten, keinen Parser. Ein Spiel besteht aus einer Welt, eine Welt aus einem
2D-Raster von Tiles und einem Pool von Entitäten. Es gilt: *pro Tile steht zu
jedem Zeitpunkt höchstens eine Entität.* Tile und Entität kennen beide die
Position, geschrieben wird sie aber ausschließlich von `RAD_WorldSpawnEntity`,
`RAD_WorldSpawnEntityWithId`, `RAD_WorldMoveEntity` und `RAD_WorldRemoveEntity` —
so kann die Doppelbuchführung nicht auseinanderlaufen.
`RAD_WorldIsConsistent` prüft sie vollständig nach.

Entitäten werden über `RAD_EntityId_t` referenziert, den Slot-Index im Pool.
Anders als ein Zeiger kann er nicht baumeln und übersteht das Speichern
unverändert.

In `game/src/control/command/` liegt daneben das Kommando: ein Anlass in
Datenform, die Absicht den Zustand zu ändern, ohne ihn schon zu ändern
([command.h](radish/game/include/radish/game/control/command/command.h)). Dazu
seine Übersetzung auf die Strecke — das Format steht geschlossen in
[codec.h](radish/game/include/radish/game/control/command/codec.h), je eine Datei
beschreibt die Nutzlast einer Kommandoart, und `byte_writer`/`byte_reader` nehmen
ihnen die Byte-Reihenfolge ab. Dieselbe Trennung wie zwischen `json_writer` und
den Serializern in `serialization/`: eine Datei beschreibt Felder, nicht Bytes.

Der Kopf jedes Kommandos trägt neben Art und Sequenznummer den **Absender**:
`RAD_UserId_t` aus [user.h](radish/game/include/radish/game/user.h), die Uuid des
Benutzers als `uint64_t`. Er steht im Kopf, weil jedes Kommando einen hat und
weil der Server ihn sonst nicht erfahren könnte — das Codefeld, mit dem Zucchini
den Rückweg kennt, ist abgeschnitten, bevor die Nutzlast ankommt. Die
Sequenznummer zählt damit je Benutzer, nicht über alle zusammen. Der Typ liegt im
Spielmodul, weil der Codec ihn schreibt und liest; wer mitspielt, weiß trotzdem
nur der Server (siehe unten) — eine Welt kennt Entitäten, keine Konten.

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
getrennt — eine `RAD_Entity_t` aus `game/` ist etwas anderes als das, was am
Bildschirm erscheint. Das Rendering bleibt Teil des Clients und keine eigene
Bibliothek: es hängt wie er an SDL und hat genau einen Nutzer.

**`server/`** ist die Gegenseite: dasselbe `radish_game`, aber statt SDL die
Zucchini-Api. Er hält den Spielzustand und wartet in einer Schleife auf
Nachrichten (`ZUC_ApiReceive`, dann `ZUC_ApiWait` auf der Wakeup-FIFO). Ein
Wakeup heißt „es liegt etwas an", nicht „genau eine Nachricht" — deshalb wird
der Ringpuffer erst leergeräumt und dann gewartet. Der Name der Zucchini-Instanz
ist standardmäßig `zucchini`, wie in `zucchini_server` selbst; ein anderer geht
als erstes Argument.

Darunter liegt `src/interface/` — die Außengrenze des Servers, in der aus einer
Nachricht ein Kommando wird und aus einer Antwort wieder eine Nachricht
([command.h](radish/server/src/include/radish/server/interface/command.h)). Das Modul kennt Zucchini
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
([execute.h](radish/server/src/include/radish/server/control/execute.h)). Die
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
[move.c](radish/server/src/control/execute/move.c), der Rest liefert
`RAD_CONTROL_ERROR_NOT_EXECUTED`. Diese Ausführenden sind wie der Roster privat:
ihr Header liegt neben der Quelle, denn sie prüfen nichts mehr — wäre einer von
außen erreichbar, ließe sich ein Zug an der Berechtigung vorbei ausführen.
Gezogen wird über `RAD_WorldMoveEntity`, die einzige Stelle, die Tile und Entität
synchron hält; ein abgelehnter Zug lässt die Welt garantiert unverändert, und
`value` sagt warum (`TARGET_OCCUPIED`, `OUT_OF_BOUNDS`, `NO_SUCH_ENTITY`).

Daneben liegt in `control/` der Loader
([loader.h](radish/server/src/include/radish/server/control/loader.h)) — woher
das Spiel kommt, an dem der Server arbeitet. Dieselbe Frage von der anderen
Seite: `execute` entscheidet, was mit dem Spielzustand geschieht, der Loader
bringt ihn hervor. `main` holt das Spiel dort und füttert damit die Steuerung,
kennt seine Herkunft also nicht. `RAD_ControlCreateGame(save_path)` legt ein
leeres Spiel an und liest, wenn ein Pfad dasteht, einen Spielstand hinein; ohne
Pfad bleibt es leer, mit einem Pfad, der nicht trägt, gibt es kein Spiel und der
Server bricht ab. Der Event-Manager, an dem das Spiel hängt, gehört dabei dem
Loader — er liegt auf dem Heap und wird in `RAD_ControlDestroyGame` mit
abgebaut, sodass der Aufrufer nichts länger am Leben halten muss als das Spiel
selbst.

Die Datei selbst nimmt ein Modul im Innern:
[loader/save_file.c](radish/server/src/control/loader/save_file.c), privat wie
alles unter `control/`. Es macht die Datei auf, misst sie gegen
`RAD_SAVE_JSON_MAX`, liest sie am Stück und gibt sie an
`RAD_DeserializeGameFromJson` — das Format steht in `serialization/` und nur
dort. Sein Ergebnis-Enum beschreibt allein die Datei (nicht zu öffnen, nicht zu
lesen, zu groß); was am *Inhalt* falsch war, reicht es als
`RAD_SerializeResult_t` unverändert durch, statt es nachzubauen. Entschieden
wird dort nichts: ob ein misslungener Ladevorgang den Server anhält, steht in
`loader.c`.

Der Zustand von `execute` ist ein unvollständiger Typ, `RAD_Control_t`, wie
`ZUC_Api_t` — angelegt mit `RAD_CreateControl`, abgebaut mit
`RAD_DestroyControl`. Darin liegt die Teilnehmerliste: `src/control/session/`,
eine Tabelle fester Größe, die zur Uuid aus dem Kommandokopf den Mitspieler und
dessen Figuren findet — und umgekehrt zur Figur ihren Besitzer. Ein Benutzer
führt beliebig viele Figuren, eine Figur gehört höchstens einem Benutzer; die
zweite Hälfte ist die wichtige, denn nur durch sie ist „darf der das bewegen?"
überhaupt entscheidbar. Die Liste je Mitspieler ist so lang wie der
Entitätenpool der Welt und kann deshalb nie voll laufen — im Grenzfall gehören
alle Figuren demselben. Ihr Header liegt
als einziger im Server **nicht** unter `src/include/`, sondern neben seiner
Quelle: der Roster ist ein Modul im Innern von `control/`. Wer einen Benutzer
anlegen oder ihm eine Figur zuordnen will, geht durch `RAD_ControlAddUser` und
`RAD_ControlBindUserEntity` — sonst ließe sich an der Prüfung vorbei ändern, wer
mitspielt und wem was gehört. Nachgeschlagen wird linear; bei acht Plätzen wäre
jede Beschleunigung teurer als die Suche. Und wie `interface/` ändert der Roster
**nie** einen Spielzustand: eine Figur entsteht im Spielmodul, hier wird nur
vermerkt, wem sie gehört. Im Server steht er und nicht in `radish_game`, weil ein
Benutzer existiert, weil eine Verbindung existiert — und davon weiß eine Welt
nichts.

**`serialization/`** bildet den Spielzustand auf JSON ab. Zu jedem Typ gibt es
einen eigenen Serializer (`tile_serializer`, `entity_serializer`,
`world_serializer`, `game_serializer`), der genau seinen Typ auf ein
JSON-Objekt abbildet und nichts darüber hinaus prüft; die Prüfung übergreifender
Zusammenhänge macht der World-Serializer. `json_writer` und `json_reader`
kapseln Formatierung und Token-Lauf, sodass die Serializer nur die Struktur
beschreiben. Einstiegspunkte sind `RAD_SerializeGameToJson` und
`RAD_DeserializeGameFromJson` in
[serialization.h](radish/serialization/include/radish/serialization/serialization.h).

Weil `game/` und `serialization/` kein SDL ziehen, lassen sich beide Bibliotheken
ohne Emscripten auf dem Host bauen — siehe [Bauen](#bauen).

Jede der beiden Bibliotheken legt ihre öffentlichen Header unter
`<projekt>/include/radish/<modul>/` ab und gibt dieses Verzeichnis `PUBLIC`
weiter. Deshalb bindet man sie überall gleich ein, egal von wo:

```c
#include <radish/game/model/world/world.h>
#include <radish/serialization/serialization.h>
```

Client und Server halten es genauso, nur liegt ihr `include/` innerhalb von
`src/` und geht `PRIVATE` statt `PUBLIC` — sie sind Programme, aus denen niemand
etwas einbindet:

```c
#include <radish/rendering/iso_map.h>          // client/src/include/
#include <radish/server/interface/command.h>   // server/src/include/
```

> Stand jetzt ruft [main.c](radish/client/src/main.c) noch ausschließlich das
> Rendering auf. Das Game-Modul und die Serialisierung sind gebaut und getestet,
> aber noch nicht angebunden.
>
> Der Server liest eingehende Nachrichten als Kommando, gibt sie an `control/` und
> schickt die Antwort zurück — `handle_message` in
> [main.c](radish/server/src/main.c). **Ausgeführt** wird davon bisher
> `move_entity`; die übrigen vier Arten werden geprüft und mit
> `RAD_CONTROL_ERROR_NOT_EXECUTED` beantwortet, ihr Ausführender fehlt noch.
>
> Der Spielzustand dazu kommt aus dem Loader: ohne Argument ein leeres Spiel —
> 8×8 Grund und die eine Figur, die `RAD_InitWorld` setzt —, mit einem zweiten
> Argument ein Spielstand aus einer JSON-Datei (`server zucchini stand.json`).
> Zugeordnet wird eine Figur bisher von niemandem
> (`RAD_ControlBindUserEntity` hat keinen Aufrufer): solange keine einen Besitzer
> hat, darf jeder Mitspieler jede ziehen.
>
> **Laden schlägt derzeit immer fehl**, und zwar an einer Stelle im Spielmodul:
> `RAD_InitWorld` setzt eine Figur auf (0,0) —
> [world.c](radish/game/src/world.c) —, und `RAD_DeserializeWorld` ruft
> `RAD_InitWorld`, bevor es die gespeicherten Entitäten setzt. Deren Id 0 ist
> dann schon vergeben, und die Datei wird mit „ungültige oder doppelte
> Entitäts-Id" abgelehnt. Ohne diese eine Zeile lädt derselbe Stand anstandslos.
>
> Fortgeschrieben wird schon die Teilnehmerliste: wer sendet, spielt mit. Ein
> Beitritt ist im Protokoll nicht vorgesehen, und eine getrennte Verbindung meldet
> Zucchini dem Server auch nicht — deshalb ruft `handle_message` für jedes
> eingehende Kommando `RAD_ControlAddUser`, und `RAD_ControlRemoveUser` hat noch
> keinen Aufrufer. `RAD_ControlBindUserEntity` auch nicht: eine Figur bekommt
> ihren Besitzer, sobald `spawn_entity` wirklich ausgeführt wird. Die Uuid im Client ist
> aus demselben Grund fest verdrahtet (`RAD_CLIENT_USER_ID` in
> [main.c](radish/client/src/main.c)): eine Anmeldung, die eine ausstellen könnte,
> gibt es nicht.
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

Das Ergebnis liegt als `build-host/server/server` im Build-Verzeichnis. Zu
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

Danach liegt der Client unter <http://localhost:8080>. Die Compose-Datei startet
drei Dienste:

| Dienst | Port | Aufgabe |
|---|---|---|
| `web` | 8080 → 80 | nginx, liefert `radish/web/` aus (mit `application/wasm` als MIME-Typ) |
| `relay` | 8765, 40000–40019/udp | Signaling und WebRTC-zu-UDP-Brücke |
| `backend` | 9999/udp | Zucchini an der UDP-Strecke und der Spielserver daran |

Der Client muss vor `docker compose up` gebaut sein — das `web`-Image kopiert
`radish/web/` beim Bauen hinein.

### Das Backend-Image

`backend` ist der einzige Dienst mit zwei Prozessen, und das aus einem Grund:
`zucchini_server` und der Spielserver reden über Shared Memory und eine FIFO,
nicht über das Netz — getrennte Container gäbe das nicht her. Der
[Einstiegspunkt](docker/backend/entrypoint.sh) startet Zucchini, setzt über den
Admin-Client den Code des Clients (`0x1`) auf die Whitelist und startet dann den
Spielserver. Ohne diesen Eintrag verwirft Zucchini jedes Paket, denn mit dem Code
merkt es sich auch, wohin die Antwort geht.

Gebaut wird [aus dem Quelltext](docker/backend/Dockerfile), mit der Wurzel des
Repositorys als Kontext: das Image braucht `radish/`, `jsmn/` und `zucchini/`
zusammen. Was nicht in den Kontext gehört, steht in
[.dockerignore](.dockerignore) — ohne die gingen die 2,1 GB aus `emsdk/` mit,
auch bei `web` und `relay`.

Nur das Backend, ohne Browser und Relay:

```bash
docker compose up --build backend
```

Zum Prüfen braucht es ein UDP-Paket aus acht Byte Code (big endian, hier `1`) und
einem Kommando dahinter — etwa `spawn_entity` mit Sequenznummer 7 von Benutzer
`1`, ein `player` auf (3,4):

```
01  00 00 00 00 00 00 00 07  00 00 00 00 00 00 00 01  01  00 03  00 04  00
│   └─ sequence 7 ────────┘  └─ user 1 ───────────┘   │   └ x ┘  └ y ┘  └ z
└ Art 1 (spawn_entity)                                └ player
```

Zurück kommen 13 Byte mit derselben Art und Sequenznummer und `value` = 0:
`01 00 00 00 00 00 00 00 07 00 00 00 00`. Das Format steht in
[codec.h](radish/game/include/radish/game/control/command/codec.h) und
[response.h](radish/game/include/radish/game/control/command/response.h). Alles,
was kein Kommando ist, wird verworfen und nur geloggt.

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
