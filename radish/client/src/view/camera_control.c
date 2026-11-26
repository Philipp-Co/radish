#include <radish/view/camera_control.h>

#include <assert.h>
#include <stddef.h>

static void RAD_CameraControlDefaultOnViewChanged(void *user_argument);
static void RAD_CameraControlClamp(RAD_CameraControl_t *control);
static int32_t RAD_CameraControlClampAxis(int32_t value, int32_t map_min, int32_t map_max, int32_t viewport);
static void RAD_CameraControlStopDragging(RAD_CameraControl_t *control);

RAD_CameraControl_t RAD_CreateCameraControl(RAD_IsoMap_t *map, int32_t viewport_width, int32_t viewport_height)
{
    RAD_CameraControl_t control = {
        .map = map,
        .viewport_width = viewport_width,
        .viewport_height = viewport_height,
        .dragging = false,
        .moved_while_dragging = false,
        .subscriber = {
            .user_argument = NULL,
            .view_changed = RAD_CameraControlDefaultOnViewChanged
        }
    };
    RAD_CameraControlClamp(&control);
    return control;
}

void RAD_CameraControlSubscribeToViewChanged(
    RAD_CameraControl_t *control,
    void *user_argument,
    RAD_CameraControlViewChanged_t view_changed
)
{
    assert(NULL != control);
    control->subscriber.user_argument = user_argument;
    control->subscriber.view_changed = view_changed;
}

bool RAD_CameraControlHandleEvent(RAD_CameraControl_t *control, const SDL_Event *event)
{
    switch(event->type)
    {
        case SDL_MOUSEBUTTONDOWN:
            if(event->button.button != SDL_BUTTON_RIGHT)
            {
                return false;
            }
            control->dragging = true;
            control->moved_while_dragging = false;
            return true;

        case SDL_MOUSEBUTTONUP:
            if(event->button.button != SDL_BUTTON_RIGHT)
            {
                return false;
            }
            RAD_CameraControlStopDragging(control);
            return true;

        case SDL_MOUSEMOTION:
            if(!control->dragging)
            {
                return false;
            }
            // Wird die Taste ausserhalb des Canvas losgelassen, kommt das
            // Loslassen nicht an. Die naechste Bewegung zeigt es trotzdem: die
            // Taste ist dann nicht mehr gedrueckt.
            if((event->motion.state & SDL_BUTTON_RMASK) == 0)
            {
                RAD_CameraControlStopDragging(control);
                return false;
            }
            // Gegen die Maus: RAD_RenderIsoObject zieht die Kamera von der
            // Position ab, so bleibt der Punkt unter dem Zeiger stehen.
            RAD_CameraControlMoveBy(control, -event->motion.xrel, -event->motion.yrel);
            control->moved_while_dragging = true;
            return false;

        default:
            return false;
    }
}

void RAD_CameraControlMoveBy(RAD_CameraControl_t *control, int32_t dx, int32_t dy)
{
    control->map->camera.x += dx;
    control->map->camera.y += dy;
    RAD_CameraControlClamp(control);
}

static void RAD_CameraControlStopDragging(RAD_CameraControl_t *control)
{
    const bool moved = control->dragging && control->moved_while_dragging;

    control->dragging = false;
    control->moved_while_dragging = false;

    if(moved)
    {
        control->subscriber.view_changed(control->subscriber.user_argument);
    }
}

static void RAD_CameraControlClamp(RAD_CameraControl_t *control)
{
    int32_t min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    RAD_IsoMapScreenBounds(control->map, &min_x, &min_y, &max_x, &max_y);

    control->map->camera.x = RAD_CameraControlClampAxis(control->map->camera.x, min_x, max_x, control->viewport_width);
    control->map->camera.y = RAD_CameraControlClampAxis(control->map->camera.y, min_y, max_y, control->viewport_height);
}

///
/// Die Kamera ist die linke bzw. obere Kante des Bildschirms in Kartenpixeln.
///
/// Karte groesser als der Bildschirm: die Kante darf von map_min bis
/// map_max - viewport laufen, der Bildschirm bleibt innerhalb der Karte.
///
/// Karte kleiner: umgekehrt, von map_max - viewport bis map_min -- die Karte
/// bleibt innerhalb des Bildschirms.
///
/// Beides ist dasselbe Intervall mit vertauschten Enden.
///
static int32_t RAD_CameraControlClampAxis(int32_t value, int32_t map_min, int32_t map_max, int32_t viewport)
{
    const int32_t a = map_min;
    const int32_t b = map_max - viewport;
    const int32_t low = (a < b) ? a : b;
    const int32_t high = (a < b) ? b : a;

    if(value < low)
    {
        return low;
    }
    if(value > high)
    {
        return high;
    }
    return value;
}

static void RAD_CameraControlDefaultOnViewChanged(void *user_argument)
{
    (void)user_argument;
}
