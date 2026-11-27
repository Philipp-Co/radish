#!/bin/bash
#
# Klont die Bibliotheken, die neben radish/ liegen muessen (siehe
# radish/cmake/*.cmake), auf genau die Staende, mit denen gebaut wird. Die
# Pipeline (.github/workflows/ci.yml) holt sie hierueber; lokal taugt es fuer
# einen frischen Checkout.
#
# Ein Klon, dessen Verzeichnis schon existiert, wird uebersprungen -- das
# Skript ueberschreibt nichts. Wer einen Klon anhebt, traegt den neuen Commit
# hier ein.
#
# Nutzung:
#   scripts/fetch-deps.sh                  # alle
#   scripts/fetch-deps.sh jsmn Unity       # nur diese
#
# Wer was braucht:
#   Host-Build mit Tests  jsmn zucchini protobuf-c Unity
#   Wasm-Client           jsmn protobuf-c SDL SDL_image
#   Image backend         jsmn protobuf-c SDL SDL_image
#   Image game-server     jsmn zucchini protobuf-c

set -euo pipefail

# Immer in die Wurzel des Repositorys klonen, egal von wo aus aufgerufen.
cd "$(dirname "$0")/.."

# URL und Commit je Klon; bei SDL/SDL_image der Commit hinter dem Release-Tag.
# Eine Funktion statt eines assoziativen Arrays: das macOS-eigene bash 3.2
# kennt keine.
spec() {
    case "$1" in
        jsmn)       echo https://github.com/zserge/jsmn.git                   25647e692c7906b96ffd2b05ca54c097948e879c ;;
        zucchini)   echo https://github.com/Philipp-Co/zucchini.git           304cdcac6f8ca88c592219ad5665c0f4b30d592d ;;
        protobuf-c) echo https://github.com/protobuf-c/protobuf-c.git         4719fdd7760624388c2c5b9d6759eb6a47490626 ;;  # v1.5.2
        Unity)      echo https://github.com/ThrowTheSwitch/Unity.git          3a6eb6dfd7706b703adf60f5ce3bcad57f94de4f ;;
        SDL)        echo https://github.com/libsdl-org/SDL.git                5d249570393f7a37e037abf22cd6012a4cc56a71 ;;  # release-2.32.10
        SDL_image)  echo https://github.com/libsdl-org/SDL_image.git          7b3347c0d90d1f1e9cc4e06e145432697ca4e68f ;;  # release-2.6.0
        *) return 1 ;;
    esac
}

ALL="jsmn zucchini protobuf-c Unity SDL SDL_image"

if [ "$#" -eq 0 ]; then
    # shellcheck disable=SC2086 # gewollt in Woerter zerlegt
    set -- $ALL
fi

name=""
for name in "$@"; do
    url_commit=""
    if ! url_commit="$(spec "$name")"; then
        echo "Unbekannt: $name (bekannt: $ALL)" >&2
        exit 1
    fi
    url="${url_commit% *}"
    commit="${url_commit#* }"
    if [ -e "$name" ]; then
        echo "$name: liegt schon da, uebersprungen"
        continue
    fi
    echo "$name: $commit"
    # Nur dieser eine Commit, ohne Historie.
    git init -q "$name"
    git -C "$name" fetch -q --depth 1 "$url" "$commit"
    git -C "$name" checkout -q FETCH_HEAD
done
