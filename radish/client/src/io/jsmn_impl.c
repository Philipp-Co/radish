///
/// Einzige Uebersetzungseinheit des Clients, die die jsmn-Implementierung erzeugt
/// -- wie game/src/serialization/jsmn_impl.c im Spiel, das der Client nicht
/// einbindet. Ueberall sonst wird jsmn.h mit gesetztem JSMN_HEADER eingebunden
/// (siehe rendering/iso_unit_sprite.c) und liefert nur die Deklarationen --
/// sonst gaebe es die Symbole mehrfach.
///
/// JSMN_STRICT lehnt alles ab, was nicht sauberes JSON ist: unquotierte Werte
/// und Primitive, die nicht Zahl, Boolean oder null sind. Fuer ein Speicher-
/// format ist ein frueher, klarer Syntaxfehler mehr wert als Nachsicht.
///
/// JSMN_PARENT_LINKS bleibt bewusst ungesetzt -- es wuerde jsmntok_t um ein Feld
/// erweitern und muesste dann in jeder Uebersetzungseinheit gleich gesetzt sein.
/// Der Leser zaehlt Teilbaeume ueber "size" ab und braucht die Elternzeiger nicht.
///
#define JSMN_STRICT
#include <jsmn.h>
