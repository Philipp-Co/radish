#ifndef __RAD_VIEW_UNIT_INFO_VIEW_H__
#define __RAD_VIEW_UNIT_INFO_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <radish/view/entity_view.h>

///
/// Ein rechteckiger Bereich im Fenster mit einer Einheit: oben ihr Name, darunter
/// nebeneinander die Summe ihrer Lebenspunkte als Balken und ihre
/// Bewegungsreichweite als Zahl, darunter je unterschiedlicher Entitaet eine
/// Zeile in zwei Spalten -- links wie viele davon in der Einheit sind ("3x"),
/// rechts eine EntityView (view/entity_view.h). Der Hintergrund ist blau.
///
///   <Name der Einheit>
///   LP [######----]  MV 4
///   3x   <EntityView>
///   1x   <EntityView>
///   ...
///
/// **Zeilen werden angehaengt** (RAD_UnitInfoViewAddEntity), jede so hoch, wie
/// ihre EntityView fuer ihre Waffen braucht (RAD_EntityViewHeight).
///
/// **Ihre Hoehe passt sich dem Inhalt an**: sie belegt den unteren Teil der
/// Flaeche aus RAD_CreateUnitInfoView, so hoch wie Name und Zeilen es brauchen,
/// und waechst mit jeder Zeile nach oben -- hoechstens bis zur Oberkante dieser
/// Flaeche. Eine Zeile, die dann nicht mehr ganz hineinpasst, wird nicht
/// gezeichnet, und alle danach auch nicht.
///
/// **Sie kennt keinen Einheiten-Typ.** Was in einer Zeile steht, setzt der
/// Aufrufer ueber die Setter der EntityView, die RAD_UnitInfoViewAddEntity
/// zurueckgibt.
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Hoechstzahl der Zeilen, also der unterschiedlichen Entitaeten.
///
#define RAD_UNIT_INFO_VIEW_ENTITIES_MAX 8

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
/// Name der Einheit (RAD_TextViewSetText).
///
bool RAD_UnitInfoViewSetName(RAD_UnitInfoView_t *unit_info_view, const char *name);

///
/// Die Lebenspunkte der ganzen Einheit: "current" ist die Summe dessen, was ihre
/// Entitaeten noch haben, "maximum" die Summe ihrer Hoechstwerte
/// (RAD_BarViewSetValue). Unabhaengig von den Lebenspunkten der einzelnen
/// EntityViews -- die Summe bildet der Aufrufer.
///
void RAD_UnitInfoViewSetHitPoints(RAD_UnitInfoView_t *unit_info_view, int32_t current, int32_t maximum);

///
/// Die Bewegungsreichweite der Einheit, in Feldern, angezeigt als "MV <Zahl>".
/// false, wenn fuer den Text kein Speicher da ist (RAD_TextViewSetText).
///
bool RAD_UnitInfoViewSetMovementRange(RAD_UnitInfoView_t *unit_info_view, int32_t movement_range);

///
/// Haengt eine Zeile an: "count" Entitaeten dieser Art, mit "number_of_weapons"
/// Waffen (hoechstens RAD_ENTITY_VIEW_WEAPONS_MAX). Die Zahl der Waffen ist damit
/// fest; die EntityView ist schon darauf gesetzt.
///
/// Die EntityView der neuen Zeile, um sie zu fuellen. Sie gehoert der
/// UnitInfoView und gilt bis RAD_UnitInfoViewClearEntities oder
/// RAD_DestroyUnitInfoView. NULL, wenn schon RAD_UNIT_INFO_VIEW_ENTITIES_MAX
/// Zeilen da sind, "number_of_weapons" zu gross ist oder kein Speicher da ist --
/// dann bleibt alles, wie es war.
///
RAD_EntityView_t* RAD_UnitInfoViewAddEntity(RAD_UnitInfoView_t *unit_info_view, uint32_t count, size_t number_of_weapons);

///
/// Aendert die Anzahl in Zeile "row". false, wenn es die Zeile nicht gibt.
///
bool RAD_UnitInfoViewSetEntityCount(RAD_UnitInfoView_t *unit_info_view, size_t row, uint32_t count);

///
/// Entfernt alle Zeilen; ihre EntityViews sind danach ungueltig.
///
void RAD_UnitInfoViewClearEntities(RAD_UnitInfoView_t *unit_info_view);

///
/// Zeichnet die UnitInfoView mit "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateUnitInfoView(RAD_UnitInfoView_t *unit_info_view, SDL_Renderer *renderer);

#endif
