#ifndef __RAD_ISO_ENTITY_H__
#define __RAD_ISO_ENTITY_H__

#include <SDL2/SDL_render.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/io/net_types.h>
#include <radish/rendering/iso_unit_images.h>

///
/// Was die Darstellung von einer Figur weiss.
///
/// **Die Id sagt, welche Figur das hier ist.** Ohne sie waere ein Iso-Objekt nur
/// etwas, das auf einem Feld liegt, und wiederzufinden allein ueber dieses Feld --
/// also nur so lange, wie es stimmt.
///
/// **Fuers Aufraeumen**: ob auf einem Feld noch dieselbe Figur liegt, ist eine
/// Frage, die nur die Id beantwortet.
///
/// **Wem sie gehoert**: einer Figur auf einem Feld der Iso-Map
/// (RAD_IsoObject_t.entities) gehoert die Map -- sie legt sie an und gibt sie
/// frei (rendering/iso_map.h). Eine Figur in Bewegung gehoert dem, der sie in
/// die Zeichenliste eingetragen hat (RAD_IsoMapAddEntity), bis er sie einem
/// Feld zurueckgibt.
///
typedef struct
{
    RAD_NetEntityId_t id;

    ///
    /// Wo die Figur steht, in Feldern -- Nachkommastellen erlaubt --, und auf
    /// welcher Ebene. Nur fuer eine Figur, die nicht auf einem Feld liegt
    /// (RAD_IsoMapAddEntity), etwa eine in Bewegung; eine ruhende zeichnet ihr
    /// Feld (RAD_RenderIsoObject), dort bleiben diese Werte ungenutzt.
    ///
    float x;
    float y;
    int32_t layer;

    ///
    /// Wie die Figur aussieht: das Bild ihres Einheitentyps
    /// (RAD_IsoUnitSpriteOf), RAD_ISO_UNIT_SPRITE_NONE fuer das Rechteck. Gesetzt
    /// von der Iso-Map, sobald eine Einheit auf das Feld kommt.
    ///
    RAD_IsoUnitSprite_t sprite;

    ///
    /// Wohin sie schaut. Nur der Client weiss davon, das Spiel kennt keine
    /// Blickrichtung: gesetzt beim Aufstellen und bei jedem Schritt eines Zuges
    /// (view/iso_map_view.c). "facing_set" ist false, bis jemand sie gesetzt hat.
    ///
    RAD_IsoFacing_t facing;
    bool facing_set;

    ///
    /// Ob sie sich gerade dreht, von "facing" nach "turn_to", und wie weit sie
    /// damit ist (0 bis 1). Gezeichnet wird dann das Einzelbild dazwischen
    /// (RAD_IsoUnitTurnFrame), sonst das ihrer Blickrichtung. Gesetzt von der
    /// Animation eines Zuges (view/iso_map_view.c); am Ende einer Drehung steht
    /// "turning" wieder auf false und "facing" auf "turn_to".
    ///
    bool turning;
    RAD_IsoFacing_t turn_to;
    float turn_progress;

    ///
    /// Wie weit ihr Turm gegen die Wanne gedreht ist, in Grad, gezaehlt wie in
    /// rendering/iso_facing.h; 0 nach vorn. Gesetzt von der Animation, wenn sie
    /// zielt (view/iso_map_view.c). Waehrend einer Drehung der Figur ist er 0 --
    /// dort zeigt der Atlas den Turm nur nach vorn.
    ///
    float turret_angle;
} RAD_IsoEntity_t;

///
/// Zeichnet die Figur "entity" mit "renderer" auf ein Feld, dessen Raute bei
/// (screen_x, screen_y) beginnt -- in Bildschirmkoordinaten, Kamera und Ebene
/// schon eingerechnet, wie RAD_RenderIsoObject sie bestimmt. Fuer eine Figur in
/// Bewegung duerfen das Koordinaten zwischen zwei Feldern sein.
///
/// Mit Bild (entity->sprite) das Einzelbild ihrer Blickrichtung aus dem Atlas
/// der Drehung, waehrend einer Drehung das dazwischen, mit gedrehtem Turm das
/// aus dessen Atlas (rendering/iso_unit_images.h), so gesetzt, dass die Figur
/// auf der Mitte der Raute steht; darunter ein weicher, flacher Schatten, wenn das Bild
/// keinen eigenen mitbringt. Ohne Bild, oder solange es nicht
/// geladen ist, ein gruenes Rechteck, so breit und hoch wie ein Feld, das eine
/// halbe Feldhoehe ueber der Raute beginnt.
///
void RAD_RenderIsoEntity(SDL_Renderer *renderer, const RAD_IsoEntity_t *entity, int32_t screen_x, int32_t screen_y);


#endif
