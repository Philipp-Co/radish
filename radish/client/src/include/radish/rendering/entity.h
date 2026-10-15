#ifndef __RAD_ISO_ENTITY_H__
#define __RAD_ISO_ENTITY_H__

#include <SDL2/SDL_render.h>
#include <stdint.h>
#include <radish/io/net_types.h>

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
} RAD_IsoEntity_t;

///
/// Zeichnet die Figur "entity" mit "renderer" auf ein Feld, dessen Raute bei
/// (screen_x, screen_y) beginnt -- in Bildschirmkoordinaten, Kamera und Ebene
/// schon eingerechnet, wie RAD_RenderIsoObject sie bestimmt. Fuer eine Figur in
/// Bewegung duerfen das Koordinaten zwischen zwei Feldern sein.
///
/// Vorerst ein gruenes Rechteck, so breit und hoch wie ein Feld, das eine halbe
/// Feldhoehe ueber der Raute beginnt. "entity" wird noch nicht ausgewertet; es
/// steht da, damit sich das Aussehen spaeter nach der Figur richten kann.
///
void RAD_RenderIsoEntity(SDL_Renderer *renderer, const RAD_IsoEntity_t *entity, int32_t screen_x, int32_t screen_y);


#endif
