#ifndef __RAD_MODEL_FOCUS_H__
#define __RAD_MODEL_FOCUS_H__

#include <radish/model/tile.h>

///
/// Was gerade im Fokus steht: das Feld unter dem Mauszeiger.
///
/// **Nur Daten**: nicht beobachtbar, ohne eigene Funktionen. Angelegt wird es
/// von main, geschrieben von der IsoMapView (view/iso_map_view.h), gelesen von
/// der TerrainInfoView (view/terrain_info_view.h).
///
typedef struct
{
    ///
    /// Das Feld im Fokus, aus der Welt (model/world.h); NULL, wenn nichts im
    /// Fokus steht. Gehoert der Welt.
    ///
    const RAD_ClientTile_t *tile;
} RAD_ClientFocus_t;

#endif
