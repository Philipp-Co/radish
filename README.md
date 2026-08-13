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
| `radish/cmake/` | CMake-Beiwerk: die Einbindung von `jsmn` und von `zucchini` |
| `radish/web/` | `index.html` und die daneben erzeugten `client.js` / `client.wasm` |
| `radish/assets/` | Schrift, wird per `--embed-file` ins Wasm-Modul eingebettet |
| `relay/` | WebRTC-zu-UDP-Relay (Python, `aiortc` + `websockets`) |
| `docker/` | Dockerfiles: Backend (Zucchini + Spielserver), Relay, nginx für `radish/web/` |

Nicht im Repository, aber zum Bauen nötig (siehe unten): `emsdk/`, `jsmn/`,
`zucchini/`.

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
([command.h](radish/server/src/interface/command.h)). Das Modul kennt Zucchini
nicht: es bekommt einen Bytebereich und schreibt in einen, ist also ohne Shared
Memory prüfbar. Und es ändert nie einen Spielzustand. Das Byteformat selbst liegt
nicht hier, sondern beim Kommando (siehe `game/`); dieses Modul ist der Adapter
darauf und dünn mit Absicht — ihm bleiben die Fragen der Nachricht: wo die Bytes
anfangen und aufhören, wie viel Platz die Antwort hat, und dass nichts
Abgeschnittenes hinausgeht. Ein eigenes Ergebnis-Enum hat es nicht: das des
Codecs geht durch, denn ein zweites daneben wäre entweder ungenauer oder eine
Kopie. Vom Rest des Servers grenzt es sich auch im Include ab,
`#include <interface/command.h>`; die Header hier tragen keinen `radish/`-Präfix,
weil sie allein zum Server gehören.

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
#include <radish/game/world.h>
#include <radish/serialization/serialization.h>
```

> Stand jetzt ruft [main.c](radish/client/src/main.c) noch ausschließlich das
> Rendering auf. Das Game-Modul und die Serialisierung sind gebaut und getestet,
> aber noch nicht angebunden.
>
> Der Server liest eingehende Nachrichten als Kommando, loggt sie und antwortet
> mit Art und Sequenznummer des Kommandos zurück — `handle_message` in
> [main.c](radish/server/src/main.c). **Ausgeführt** wird noch nichts: der
> Spielzustand bleibt unberührt, und `value` in der Antwort trägt vorläufig nur
> das Ergebnis des Lesens. Sobald Kommandos ausgeführt werden, gehört dort das
> Ergebnis des Ausführens hin.
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
einem Kommando dahinter — etwa `spawn_entity` mit Sequenznummer 7, ein `player`
auf (3,4):

```
01  00 00 00 00 00 00 00 07  01  00 03  00 04  00
│   └─ sequence 7 ────────┘  │   └ x ┘  └ y ┘  └ z
└ Art 1 (spawn_entity)       └ player
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
