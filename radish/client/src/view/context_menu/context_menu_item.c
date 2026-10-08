#include <radish/view/context_menu/context_menu_item.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


///
/// In wie viele Dreiecke der Kreis zerlegt wird (RAD_UpdateContextMenuItem).
///
#define RAD_CONTEXT_MENU_ITEM_SEGMENTS 32

///
/// In dieser Farbe wird er gezeichnet, solange er hervorgehoben ist
/// (RAD_ContextMenuItemSetHighlighted): blau.
///
#define RAD_CONTEXT_MENU_ITEM_HIGHLIGHT_COLOR ((SDL_Color){ 0, 96, 255, 255 })


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_ContextMenuItem
{
    ///
    /// Wo er auf dem Ring steht, in Grad: 0 oben, im Uhrzeigersinn. Gelesen
    /// wird das nur von der ContextMenuView.
    ///
    float angle;
    float center_x;
    float center_y;
    int32_t radius;
    SDL_Color color;
    RAD_ContextMenuItemAction_t action;

    ///
    /// Ob er hervorgehoben ist; dann wird er statt in "color" in
    /// RAD_CONTEXT_MENU_ITEM_HIGHLIGHT_COLOR gezeichnet.
    ///
    bool highlighted;

    ///
    /// Ob die Maus ueber ihm ist (RAD_ContextMenuItemHandleMouseMotion).
    ///
    bool hovered;
};


RAD_ContextMenuItem_t* RAD_CreateContextMenuItem(float angle, int32_t radius, SDL_Color color)
{
    RAD_ContextMenuItem_t *item = malloc(sizeof(struct RAD_ContextMenuItem));
    if(item == NULL)
    {
        return NULL;
    }

    *item = (struct RAD_ContextMenuItem){
        .angle = angle,
        .center_x = 0.0f,
        .center_y = 0.0f,
        .radius = radius,
        .color = color,
        .action = RAD_CONTEXT_MENU_ITEM_ACTION_NONE,
        .highlighted = false,
        .hovered = false
    };

    return item;
}

void RAD_DestroyContextMenuItem(RAD_ContextMenuItem_t **item)
{
    free(*item);
    *item = NULL;
}

float RAD_ContextMenuItemAngle(const RAD_ContextMenuItem_t *item)
{
    return item->angle;
}

void RAD_ContextMenuItemSetCenter(RAD_ContextMenuItem_t *item, float center_x, float center_y)
{
    item->center_x = center_x;
    item->center_y = center_y;
}

void RAD_ContextMenuItemSetAction(RAD_ContextMenuItem_t *item, RAD_ContextMenuItemAction_t action)
{
    item->action = action;
}

RAD_ContextMenuItemAction_t RAD_ContextMenuItemAction(const RAD_ContextMenuItem_t *item)
{
    return item->action;
}

void RAD_ContextMenuItemSetColor(RAD_ContextMenuItem_t *item, SDL_Color color)
{
    item->color = color;
}

bool RAD_ContextMenuItemContains(const RAD_ContextMenuItem_t *item, int32_t x, int32_t y)
{
    const float dx = (float)x - item->center_x;
    const float dy = (float)y - item->center_y;
    const float r = (float)item->radius;
    return dx * dx + dy * dy <= r * r;
}

void RAD_ContextMenuItemSetHighlighted(RAD_ContextMenuItem_t *item, bool highlighted)
{
    item->highlighted = highlighted;
}

bool RAD_ContextMenuItemIsHovered(const RAD_ContextMenuItem_t *item)
{
    return item->hovered;
}

void RAD_ContextMenuItemHandleMouseMotion(RAD_ContextMenuItem_t *item, const SDL_MouseMotionEvent *motion)
{
    const bool hovered = RAD_ContextMenuItemContains(item, motion->x, motion->y);
    if(hovered == item->hovered)
    {
        return;
    }

    item->hovered = hovered;
    printf("[ContextMenuItem] Maus %s Eintrag bei %.0f Grad\n",
           hovered ? "ueber" : "verlaesst", (double)item->angle);
}

bool RAD_ContextMenuItemHandleMouseButton(RAD_ContextMenuItem_t *item, const SDL_MouseButtonEvent *button)
{
    if(!RAD_ContextMenuItemContains(item, button->x, button->y))
    {
        return false;
    }

    printf("[ContextMenuItem] Maustaste %d %s auf Eintrag bei %.0f Grad\n",
           (int)button->button, (button->state == SDL_PRESSED) ? "gedrueckt" : "losgelassen", (double)item->angle);
    return true;
}

///
/// Ein gefuellter Kreis als Faecher aus RAD_CONTEXT_MENU_ITEM_SEGMENTS Dreiecken
/// um den Mittelpunkt, in einem Aufruf von SDL_RenderGeometry -- in seiner
/// Farbe, hervorgehoben in RAD_CONTEXT_MENU_ITEM_HIGHLIGHT_COLOR. Den
/// Blend-Modus setzt die ContextMenuView.
///
void RAD_UpdateContextMenuItem(RAD_ContextMenuItem_t *item, SDL_Renderer *renderer)
{
    const SDL_Color color = item->highlighted ? RAD_CONTEXT_MENU_ITEM_HIGHLIGHT_COLOR : item->color;

    // Platz 0 ist der Mittelpunkt, 1 bis SEGMENTS der Rand.
    SDL_Vertex vertices[RAD_CONTEXT_MENU_ITEM_SEGMENTS + 1];
    int indices[3 * RAD_CONTEXT_MENU_ITEM_SEGMENTS];
    const float radius = (float)item->radius;

    vertices[0] = (SDL_Vertex){
        .position = { .x = item->center_x, .y = item->center_y },
        .color = color,
        .tex_coord = { .x = 0.0f, .y = 0.0f }
    };
    for(int i = 0; i < RAD_CONTEXT_MENU_ITEM_SEGMENTS; i++)
    {
        const float angle = 2.0f * (float)M_PI * (float)i / (float)RAD_CONTEXT_MENU_ITEM_SEGMENTS;
        vertices[i + 1] = (SDL_Vertex){
            .position = { .x = item->center_x + radius * cosf(angle), .y = item->center_y + radius * sinf(angle) },
            .color = color,
            .tex_coord = { .x = 0.0f, .y = 0.0f }
        };

        indices[3 * i] = 0;
        indices[3 * i + 1] = i + 1;
        indices[3 * i + 2] = (i + 1) % RAD_CONTEXT_MENU_ITEM_SEGMENTS + 1;
    }

    SDL_RenderGeometry(renderer, NULL, vertices, RAD_CONTEXT_MENU_ITEM_SEGMENTS + 1, indices, 3 * RAD_CONTEXT_MENU_ITEM_SEGMENTS);
}
