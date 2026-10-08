#ifndef __RAD_IO_CAMERA_CONTROL_H__
#define __RAD_IO_CAMERA_CONTROL_H__

#include <SDL2/SDL_events.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/rendering/iso_map.h>

///
/// Steuert die Kamera der Iso-Map: Ziehen mit gedrueckter rechter Maustaste und
/// die Pfeiltasten.
///
/// **Rechts und nicht links**, weil die linke Taste der RootView gehoert
/// (view/view.h): ihr Loslassen waehlt ein Feld fuer die ContextMenuView aus.
/// Mit der rechten Taste kommen sich Ziehen und Klicken nicht in die Quere.
///
/// **Die Kamera bleibt an der Karte.** Nach jeder Bewegung wird sie begrenzt:
/// ist die Karte kleiner als der Bildschirm, laesst sie sich nur so weit
/// schieben, dass sie ganz zu sehen bleibt; ist sie groesser, nur so weit, dass
/// der Bildschirm nicht ueber ihren Rand hinaus zeigt.
///
/// **view_changed meldet, wenn eine Bewegung abgeschlossen ist** -- beim
/// Loslassen der Taste und nach einem Tastendruck, nicht bei jedem Pixel
/// dazwischen. Dann ist ein anderer Ausschnitt zu sehen, und main.c fragt ihn
/// beim Server an.
///

typedef void (*RAD_IoCameraControlViewChanged_t)(void *user_argument);

typedef struct
{
    RAD_IsoMap_t *map;

    ///
    /// Groesse des Bildschirms in Pixeln, gegen die begrenzt wird.
    ///
    int32_t viewport_width;
    int32_t viewport_height;

    bool dragging;

    ///
    /// Ob sich die Kamera seit Beginn des Ziehens bewegt hat -- ein Rechtsklick
    /// ohne Bewegung ist keine neue Ansicht.
    ///
    bool moved_while_dragging;

    struct
    {
        void *user_argument;
        RAD_IoCameraControlViewChanged_t view_changed;
    } subscriber;
} RAD_IoCameraControl_t;

///
/// Legt die Steuerung fuer "map" an und begrenzt die Kamera dabei gleich
/// einmal -- sie steht danach in jedem Fall innerhalb der Grenzen.
///
RAD_IoCameraControl_t RAD_CreateIoCameraControl(RAD_IsoMap_t *map, int32_t viewport_width, int32_t viewport_height);

void RAD_IoCameraControlSubscribeToViewChanged(
    RAD_IoCameraControl_t *control,
    void *user_argument,
    RAD_IoCameraControlViewChanged_t view_changed
);

///
/// Wertet ein SDL-Ereignis aus. true, wenn es damit erledigt ist und sonst
/// niemand es auswerten soll -- das gilt fuer die rechte Maustaste und die
/// Pfeiltasten. Mausbewegungen liefern immer false: auch waehrend des Ziehens
/// soll der Fokus dem Mauszeiger folgen.
///
bool RAD_IoCameraControlHandleEvent(RAD_IoCameraControl_t *control, const SDL_Event *event);

///
/// Verschiebt die Kamera um (dx, dy) Pixel und begrenzt sie danach.
///
void RAD_IoCameraControlMoveBy(RAD_IoCameraControl_t *control, int32_t dx, int32_t dy);

#endif
