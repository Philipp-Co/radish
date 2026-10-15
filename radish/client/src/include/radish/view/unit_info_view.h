#ifndef __RAD_VIEW_UNIT_INFO_VIEW_H__
#define __RAD_VIEW_UNIT_INFO_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///
/// Ein rechteckiger Bereich im Fenster mit einer Einheit, alles als Text: oben
/// ihr Name, darunter nebeneinander die Summe ihrer Lebenspunkte und ihre
/// Bewegungsreichweite, darunter eine Liste von Zeilen -- jede entweder buendig
/// oder eingerueckt. Der Hintergrund ist schwarz, mit einem weissen Rahmen.
///
///   <Name der Einheit>
///   LP 12/15     MV 4
///   3x Soldat
///      Gewehr
///      2x Granate
///   1x Sergeant
///      Pistole
///   ...
///
/// **Zeilen werden angehaengt** (RAD_UnitInfoViewAddLine), jede eine Zeile der
/// Schrift hoch. Eine eingerueckte beginnt dort, wo in einer buendigen der Text
/// nach "99x " steht.
///
/// **Ihre Hoehe passt sich dem Inhalt an**: sie belegt den unteren Teil der
/// Flaeche aus RAD_CreateUnitInfoView, so hoch wie Kopf und Zeilen es brauchen,
/// und waechst mit jeder Zeile nach oben -- hoechstens bis zur Oberkante dieser
/// Flaeche. Eine Zeile, die dann nicht mehr ganz hineinpasst, wird nicht
/// gezeichnet, und alle danach auch nicht. Was nicht in die Breite passt, wird
/// abgeschnitten (view/typewriter_view.h).
///
/// **Jeder Text tippt sich neu**, sobald er gesetzt wird, und jede Zeile, sobald
/// sie angehaengt wird (view/typewriter_view.h).
///
/// **Sie kennt keinen Einheiten-Typ.** Was in den Zeilen steht, setzt der
/// Aufrufer (view/view.h).
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Hoechstzahl der Zeilen: genug fuer jedes Mitglied einer Einheit eine Zeile
/// und darunter eine je Waffe (model/unit.h).
///
#define RAD_UNIT_INFO_VIEW_LINES_MAX 50

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in view/unit_info_view.c.
///
typedef struct RAD_UnitInfoView RAD_UnitInfoView_t;

///
/// Legt eine UnitInfoView an, die hoechstens die Flaeche mit der oberen linken
/// Ecke bei (x, y) und width x height Pixeln belegt -- leer nur deren untersten
/// Streifen fuer Namen, Lebenspunkte und Bewegungsreichweite. Mit leerem Namen,
/// 0 von 0 Lebenspunkten, Bewegungsreichweite 0 und ohne Zeilen; NULL, wenn kein
/// Speicher da ist.
/// "font" gehoert dem Aufrufer (view/view.h, RAD_ViewFont) und muss die
/// UnitInfoView ueberleben. RAD_DestroyUnitInfoView baut alles ab und nullt den
/// Zeiger des Aufrufers.
///
RAD_UnitInfoView_t* RAD_CreateUnitInfoView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font);
void RAD_DestroyUnitInfoView(RAD_UnitInfoView_t **unit_info_view);

///
/// Name der Einheit (RAD_TypewriterViewSetText).
///
bool RAD_UnitInfoViewSetName(RAD_UnitInfoView_t *unit_info_view, const char *name);

///
/// Die Lebenspunkte der ganzen Einheit, angezeigt als "LP <current>/<maximum>":
/// "current" ist die Summe dessen, was ihre Mitglieder noch haben, "maximum" die
/// Summe ihrer Hoechstwerte -- die Summe bildet der Aufrufer. false, wenn fuer
/// den Text kein Speicher da ist (RAD_TypewriterViewSetText).
///
bool RAD_UnitInfoViewSetHitPoints(RAD_UnitInfoView_t *unit_info_view, int32_t current, int32_t maximum);

///
/// Die Bewegungsreichweite der Einheit, in Feldern, angezeigt als "MV <Zahl>".
/// false, wenn fuer den Text kein Speicher da ist (RAD_TypewriterViewSetText).
///
bool RAD_UnitInfoViewSetMovementRange(RAD_UnitInfoView_t *unit_info_view, int32_t movement_range);

///
/// Haengt eine Zeile mit "text" an, eingerueckt oder buendig. false, wenn schon
/// RAD_UNIT_INFO_VIEW_LINES_MAX Zeilen da sind oder kein Speicher da ist --
/// dann bleibt alles, wie es war.
///
bool RAD_UnitInfoViewAddLine(RAD_UnitInfoView_t *unit_info_view, const char *text, bool indented);

///
/// Entfernt alle Zeilen.
///
void RAD_UnitInfoViewClearLines(RAD_UnitInfoView_t *unit_info_view);

///
/// Zeichnet die UnitInfoView mit "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateUnitInfoView(RAD_UnitInfoView_t *unit_info_view, SDL_Renderer *renderer);

#endif
