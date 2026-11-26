#ifndef __RAD_MODEL_DEFINITIONS_H__
#define __RAD_MODEL_DEFINITIONS_H__

///
/// Wie gross das Raster der Welt ist, in Feldern -- quadratisch
/// (RAD_ClientWorld_t.tiles, model/world.h). Ein Feld ausserhalb davon wird
/// verworfen.
///
/// **Hier und nicht bei der Darstellung**: wie viele Felder es gibt, sagt die
/// Welt; die Iso-Map legt ihr Raster danach an (view/rendering/iso_map.h). Umgekehrt
/// haette das Modell an der Darstellung gehangen.
///
/// Der Name traegt noch das "ISO" aus seiner alten Heimat
/// (view/rendering/iso_definitions.h), wo neben ihm die Kachelmasse stehen.
///
#define RAD_ISO_MAP_SIZE 16

#endif
