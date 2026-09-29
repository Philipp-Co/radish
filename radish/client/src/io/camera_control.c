#include <radish/io/camera_control.h>

#include <assert.h>
#include <stddef.h>

///
/// Um so viele Pixel verschiebt ein Druck auf eine Pfeiltaste die Kamera.
///
#define RAD_IO_CAMERA_KEY_STEP 25

static void RAD_IoCameraControlDefaultOnViewChanged(void *user_argument);
static void RAD_IoCameraControlClamp(RAD_IoCameraControl_t *control);
static int32_t RAD_IoCameraControlClampAxis(int32_t value, int32_t map_min, int32_t map_max, int32_t viewport);
static void RAD_IoCameraControlStopDragging(RAD_IoCameraControl_t *control);

RAD_IoCameraControl_t RAD_CreateIoCameraControl(RAD_IsoMap_t *map, int32_t viewport_width, int32_t viewport_height)
{
    RAD_IoCameraControl_t control = {
        .map = map,
        .viewport_width = viewport_width,
        .viewport_height = viewport_height,
        .dragging = false,
        .moved_while_dragging = false,
        .subscriber = {
            .user_argument = NULL,
            .view_changed = RAD_IoCameraControlDefaultOnViewChanged
        }
    };
    RAD_IoCameraControlClamp(&control);
    return control;
}

void RAD_IoCameraControlSubscribeToViewChanged(
    RAD_IoCameraControl_t *control,
    void *user_argument,
    RAD_IoCameraControlViewChanged_t view_changed
)
{
    assert(NULL != control);
    control->subscriber.user_argument = user_argument;
    control->subscriber.view_changed = view_changed;
}

bool RAD_IoCameraControlHandleEvent(RAD_IoCameraControl_t *control, const SDL_Event *event)
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
            RAD_IoCameraControlStopDragging(control);
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
                RAD_IoCameraControlStopDragging(control);
                return false;
            }
            // Gegen die Maus: RAD_RenderIsoObject zieht die Kamera von der
            // Position ab, so bleibt der Punkt unter dem Zeiger stehen.
            RAD_IoCameraControlMoveBy(control, -event->motion.xrel, -event->motion.yrel);
            control->moved_while_dragging = true;
            return false;

        case SDL_KEYUP:
            switch(event->key.keysym.scancode)
            {
                case SDL_SCANCODE_UP:
                    RAD_IoCameraControlMoveBy(control, 0, RAD_IO_CAMERA_KEY_STEP);
                    break;
                case SDL_SCANCODE_DOWN:
                    RAD_IoCameraControlMoveBy(control, 0, -RAD_IO_CAMERA_KEY_STEP);
                    break;
                case SDL_SCANCODE_LEFT:
                    RAD_IoCameraControlMoveBy(control, RAD_IO_CAMERA_KEY_STEP, 0);
                    break;
                case SDL_SCANCODE_RIGHT:
                    RAD_IoCameraControlMoveBy(control, -RAD_IO_CAMERA_KEY_STEP, 0);
                    break;
                default:
                    return false;
            }
            control->subscriber.view_changed(control->subscriber.user_argument);
            return true;

        default:
            return false;
    }
}

void RAD_IoCameraControlMoveBy(RAD_IoCameraControl_t *control, int32_t dx, int32_t dy)
{
    control->map->camera.x += dx;
    control->map->camera.y += dy;
    RAD_IoCameraControlClamp(control);
}

static void RAD_IoCameraControlStopDragging(RAD_IoCameraControl_t *control)
{
    const bool moved = control->dragging && control->moved_while_dragging;

    control->dragging = false;
    control->moved_while_dragging = false;

    if(moved)
    {
        control->subscriber.view_changed(control->subscriber.user_argument);
    }
}

static void RAD_IoCameraControlClamp(RAD_IoCameraControl_t *control)
{
    int32_t min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    RAD_IsoMapScreenBounds(control->map, &min_x, &min_y, &max_x, &max_y);

    control->map->camera.x = RAD_IoCameraControlClampAxis(control->map->camera.x, min_x, max_x, control->viewport_width);
    control->map->camera.y = RAD_IoCameraControlClampAxis(control->map->camera.y, min_y, max_y, control->viewport_height);
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
static int32_t RAD_IoCameraControlClampAxis(int32_t value, int32_t map_min, int32_t map_max, int32_t viewport)
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

static void RAD_IoCameraControlDefaultOnViewChanged(void *user_argument)
{
    (void)user_argument;
}
