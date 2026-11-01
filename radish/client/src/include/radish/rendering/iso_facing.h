#ifndef __RAD_RENDERING_ISO_FACING_H__
#define __RAD_RENDERING_ISO_FACING_H__

#include <stdint.h>

///
/// Wohin eine Figur schaut, im Raster: die Richtung, in die sie mit dem naechsten
/// Schritt zoege. Auf dem Bildschirm (RAD_ToIsoCoordinates) ist +x rechts oben,
/// +y rechts unten, -x links unten, -y links oben.
///
/// **Die Reihenfolge ist die einer Drehung**: von einer Richtung zur naechsten
/// ist es eine Vierteldrehung, immer im selben Sinn, nach Y_MINUS wieder
/// X_MINUS. In dieser Reihenfolge liegen auch die Bilder einer Drehung im Atlas
/// (rendering/iso_unit_images.h).
///
typedef enum
{
    RAD_ISO_FACING_X_MINUS = 0,
    RAD_ISO_FACING_Y_PLUS,
    RAD_ISO_FACING_X_PLUS,
    RAD_ISO_FACING_Y_MINUS,
    RAD_ISO_FACINGS
} RAD_IsoFacing_t;

///
/// Die Blickrichtung fuer einen Schritt um (dx, dy) Felder: entlang der
/// laengeren Achse, bei gleich langen entlang x. "current", wenn beide 0 sind.
///
RAD_IsoFacing_t RAD_IsoFacingOf(float dx, float dy, RAD_IsoFacing_t current);

///
/// Wie viele Vierteldrehungen es von "from" nach "to" sind, auf dem kuerzeren
/// Weg: 0, 1 (in Richtung der Aufzaehlung), -1 (dagegen) oder 2 (Kehrtwende, in
/// Richtung der Aufzaehlung). 0 auch, wenn eine der beiden ausserhalb der
/// Aufzaehlung liegt.
///
int32_t RAD_IsoFacingQuarterTurns(RAD_IsoFacing_t from, RAD_IsoFacing_t to);

///
/// Winkel im Raster, in Grad, gezaehlt wie die Aufzaehlung: X_MINUS 0, Y_PLUS
/// 90, X_PLUS 180, Y_MINUS 270. So liegen auch die Bilder einer Drehung im
/// Atlas (rendering/iso_unit_images.h) -- gilt fuer Wanne und Turm.
///

///
/// Der Winkel der Blickrichtung "facing"; 0 ausserhalb der Aufzaehlung.
///
float RAD_IsoFacingAngle(RAD_IsoFacing_t facing);

///
/// Der Winkel, unter dem ein Feld um (dx, dy) Felder entfernt liegt, in
/// [0, 360) -- genau, nicht auf eine Blickrichtung gerundet. 0, wenn beide 0
/// sind.
///
float RAD_IsoFacingAngleOf(float dx, float dy);

///
/// "angle" auf [0, 360) gebracht.
///
float RAD_IsoAngleNormalize(float angle);

///
/// Um wie viel Grad es von "from" nach "to" ist, auf dem kuerzeren Weg: in
/// (-180, 180], positiv in Richtung der Aufzaehlung.
///
float RAD_IsoAngleDelta(float from, float to);

#endif
