#ifndef __RAD_GAME_CONTROL_VISIBILITY_H__
#define __RAD_GAME_CONTROL_VISIBILITY_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/game/game.h>
#include <radish/game/game_definitions.h>

///
/// Die Sicht: was ein Spieler vom Feld weiss, weil eine seiner Einheiten es
/// sieht -- die Grundlage fuer den Nebel des Krieges.
///
/// **Privat, mit Absicht.** Wer was erfahren darf, entscheidet das Spiel; nach
/// aussen soll spaeter nur das Ergebnis gehen (Ereignisse je Beobachter, ein
/// gefiltertes Lesen), nicht die Regel. Deshalb darf hier auch die Groesse der
/// Welt stehen (game_definitions.h), die sonst nicht hinaus soll.
///
/// **Die Regel:** eine Einheit sieht ein Feld, wenn sie auf dem Feld steht, es
/// hoechstens RAD_Unit_t.sight_range Felder entfernt ist -- waagerecht plus
/// senkrecht, wie die Reichweite einer Waffe (RAD_GameDistance) -- und die
/// Sichtlinie frei ist. Es ist dieselbe Sichtlinie wie beim Angriff
/// (RAD_GameHasLineOfSight): Gelaende und Einheiten dazwischen versperren, das
/// Feld selbst nicht. Eine Einheit, die die Sicht versperrt, sieht man also
/// selbst; ihr eigenes Feld sieht jede Einheit.
///
/// **Kein Zustand.** Jede Abfrage rechnet aus der Welt, wie sie gerade ist. Bei
/// hoechstens RAD_WORLD_WIDTH x RAD_WORLD_HEIGHT Feldern und RAD_MAX_UNITS
/// Einheiten waere ein gespeichertes Raster nur ein zweites Buch, das bei jedem
/// Zug nachgefuehrt werden muesste.
///
/// Eine Stelle ausserhalb der Welt, ein NULL-Spiel oder eine unbekannte Id sind
/// keine Fehler, sondern "sieht nicht": false, ohne assert.
///

///
/// Was ein Spieler sieht, Feld fuer Feld: tiles[y][x], dieselbe Reihenfolge wie
/// RAD_World_t.tiles. Belegt ist width x height, die Groesse der Welt; alles
/// dahinter ist false.
///
typedef struct
{
    int32_t width;
    int32_t height;
    bool tiles[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH];
} RAD_Visibility_t;

///
/// Sieht die Einheit "unit" das Feld (x, y)? false, wenn es sie nicht gibt oder
/// sie nicht auf dem Feld steht -- aus der Reserve und zerstoert sieht keine.
///
bool RAD_GameUnitSeesTile(const RAD_Game_t *game, RAD_UnitId_t unit, int32_t x, int32_t y);

///
/// Sieht "user" das Feld (x, y), mit wenigstens einer seiner Einheiten? Einheiten
/// ohne Besitzer sehen fuer niemanden: fuer RAD_USER_NONE immer false.
///
bool RAD_GameIsTileVisible(const RAD_Game_t *game, RAD_UserId_t user, int32_t x, int32_t y);

///
/// Sieht "user" die Einheit "unit"? Eine eigene immer, gleich wo sie steht. Eine
/// fremde nur, wenn sie auf dem Feld steht und "user" ihr Feld sieht -- in der
/// Reserve oder zerstoert ist sie nicht zu sehen.
///
bool RAD_GameIsUnitVisible(const RAD_Game_t *game, RAD_UserId_t user, RAD_UnitId_t unit);

///
/// Alles, was "user" sieht, auf einmal -- je Feld dieselbe Antwort wie
/// RAD_GameIsTileVisible. Gedacht fuer den Vergleich vorher/nachher, wenn sich
/// die Welt aendert. false fuer ein NULL-Spiel oder NULL als "output"; "output"
/// bleibt dann unberuehrt.
///
bool RAD_GameVisibility(const RAD_Game_t *game, RAD_UserId_t user, RAD_Visibility_t *output);

///
/// Wer von einer Aenderung auf (x, y) erfaehrt: "owner" -- der Besitzer dessen,
/// was sich aendert, er erfaehrt es immer -- und jeder Mitspieler, der das Feld
/// sieht. "output" wird dabei neu gefuellt, nicht ergaenzt. RAD_USER_NONE als
/// Besitzer steht nicht darin, eine Stelle ausserhalb der Welt sieht niemand.
/// false nur fuer NULL.
///
bool RAD_GameRecipientsForTile(const RAD_Game_t *game, RAD_UserId_t owner, int32_t x, int32_t y,
                               RAD_Recipients_t *output);

///
/// Was "user" von einem Weg sieht: die Felder von "path", die er sieht, in ihrer
/// Reihenfolge -- in "output", und ihre Anzahl als Rueckgabe. Liegen
/// unsichtbare dazwischen, fehlen sie einfach; der Weg kann dann springen.
///
/// Gerechnet wird mit der Welt, wie sie jetzt ist, also nach dem Zug. Steht die
/// gezogene Einheit am Ende so, dass sie einem Beobachter die Sicht auf ein Feld
/// verstellt, ueber das sie vorher gezogen ist, fehlt dieses Feld -- obwohl er
/// es waehrend des Zuges gesehen haette. Eine bekannte Ungenauigkeit; genau
/// waere es nur, den Zug Feld fuer Feld nachzurechnen.
///
/// 0 und "output" leer fuer NULL.
///
int32_t RAD_GameVisiblePartOfPath(const RAD_Game_t *game, RAD_UserId_t user, const RAD_Path_t *path,
                                  RAD_Path_t *output);

///
/// Was ein Beobachter von einem fremden Zug gemeldet bekommt
/// (control/move_unit.c): der sichtbare Teil (RAD_GameVisiblePartOfPath) und
/// dazu je ein **Randfeld** -- das Feld des Weges direkt vor dem ersten
/// sichtbaren und das direkt nach dem letzten, soweit es sie gibt.
///
/// So faehrt eine Einheit, die die Sicht verlaesst, noch ein Feld in den Nebel,
/// und eine, die hereinkommt, faehrt ein Feld aus ihm heraus -- statt
/// wegzuspringen oder aufzutauchen. Schon ein einziges sichtbares Feld ergibt
/// damit einen Weg aus zwei. Der Preis: der Beobachter erfaehrt ein Feld weit,
/// wohin sie ging oder woher sie kam.
///
/// Sieht er kein Feld des Weges, ist der Weg leer: dann gibt es auch keinen Rand.
/// 0 und "output" leer fuer NULL.
///
int32_t RAD_GameObservedPartOfPath(const RAD_Game_t *game, RAD_UserId_t user, const RAD_Path_t *path,
                                   RAD_Path_t *output);

///
/// Wer welche Einheit sieht, festgehalten vor einem Kommando -- damit danach
/// gemeldet werden kann, was jemandem neu in die Sicht kam und was aus ihr
/// verschwand (RAD_GameNotifySightChanges). Je Mitspieler und je Platz im Pool,
/// dazu Zustand und Feld jeder Einheit.
///
typedef struct
{
    RAD_UserId_t players[RAD_MAX_PLAYERS];
    int32_t number_of_players;
    bool seen[RAD_MAX_PLAYERS][RAD_MAX_UNITS];
    RAD_UnitState_t state[RAD_MAX_UNITS];
    RAD_Position_t position[RAD_MAX_UNITS];
    int32_t number_of_units;
} RAD_SightSnapshot_t;

///
/// Haelt fest, wer jetzt welche Einheit sieht (RAD_GameIsUnitVisible). Fuer ein
/// NULL-Spiel ist der Stand leer.
///
void RAD_GameTakeSightSnapshot(const RAD_Game_t *game, RAD_SightSnapshot_t *output);

///
/// Vergleicht die Sicht jetzt mit "before" und meldet je Mitspieler, was sich
/// geaendert hat, ueber die Welt (RAD_WorldNotifyUnitSighted/Lost):
///
///   in Sicht      eine fremde Einheit, die vorher und jetzt auf dem Feld
///                 steht, sieht er jetzt und vorher nicht -- mit ihrem Feld
///   aus der Sicht eine solche sah er vorher und jetzt nicht mehr -- mit dem
///                 Feld, auf dem er sie zuletzt sah
///
/// **Nur, was sonst niemand meldet.** Eine Einheit, die gerade aufgestellt
/// wurde, kommt mit dem Aufstellen (RAD_OnUnitSpawned_t) zu denen, die ihr Feld
/// sehen; eine zerstoerte geht mit der Zerstoerung (RAD_OnUnitDestroyed_t) --
/// beide stehen nicht beide Male auf dem Feld und werden hier uebergangen.
///
/// **Wo er sie zuletzt sah:** fuer eine Einheit, die stehen blieb, ihr Feld von
/// vorher. Ist sie gezogen -- "moved" ist dann ihre Id und "path" der
/// angefragte Weg, von dem sie einen Anfang gegangen ist --, das letzte Feld
/// dieses Anfangs, das er sieht (RAD_GameVisiblePartOfPath), und sieht er
/// keins, wieder ihr Feld von vorher. Nie ihr jetziges: das sieht er nicht.
/// Kein Zug: "moved" RAD_UNIT_NONE, "path" NULL.
///
/// "in Sicht" geht je Einheit einmal an alle, die sie neu sehen; "aus der Sicht"
/// je Spieler einzeln, denn wo er sie zuletzt sah, ist fuer jeden ein anderes
/// Feld.
///
void RAD_GameNotifySightChanges(RAD_Game_t *game, const RAD_SightSnapshot_t *before,
                                RAD_UnitId_t moved, const RAD_Path_t *path);

#endif
