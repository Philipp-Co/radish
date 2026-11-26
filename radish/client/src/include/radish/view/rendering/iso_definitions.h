#ifndef __RAD_ISO_DEFINITIONS_H__
#define __RAD_ISO_DEFINITIONS_H__

///
/// Wie gross eine Kachel auf dem Bildschirm ist: die Raute ihrer Oberseite,
/// doppelt so breit wie hoch. Danach rechnet alles, was zeichnet
/// (RAD_ToIsoCoordinates).
///
/// Wie viele Kacheln es gibt, steht nicht hier, sondern bei der Welt
/// (RAD_ISO_MAP_SIZE, model/definitions.h).
///
#define RAD_ISO_TILE_HEIGHT (128)
#define RAD_ISO_TILE_WIDTH (256)

#endif
