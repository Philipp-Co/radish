#include <radish/view/context_menu/context_menu_item.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


///
/// In wie viele Dreiecke der Kreis zerlegt wird (RAD_UpdateContextMenuItem).
///
#define RAD_CONTEXT_MENU_ITEM_SEGMENTS 32

///
/// Sein Kreis ist schwarz, solange er nicht hervorgehoben ist, und blau, solange
/// er es ist (RAD_ContextMenuItemSetHighlighted); sein Symbol ist immer weiss.
///
#define RAD_CONTEXT_MENU_ITEM_BACKGROUND_COLOR ((SDL_Color){ 0, 0, 0, 255 })
#define RAD_CONTEXT_MENU_ITEM_HIGHLIGHT_COLOR ((SDL_Color){ 0, 96, 255, 255 })
#define RAD_CONTEXT_MENU_ITEM_ICON_COLOR ((SDL_Color){ 255, 255, 255, 255 })


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
    RAD_ContextMenuItemAction_t action;

    ///
    /// Ob er hervorgehoben ist (RAD_CONTEXT_MENU_ITEM_HIGHLIGHT_COLOR).
    ///
    bool highlighted;

    ///
    /// Ob die Maus ueber ihm ist (RAD_ContextMenuItemHandleMouseMotion).
    ///
    bool hovered;
};


static void RAD_ContextMenuItemFillDisc(SDL_Renderer *renderer, float center_x, float center_y, float radius, SDL_Color color);
static void RAD_ContextMenuItemFillRing(SDL_Renderer *renderer, float center_x, float center_y, float inner_radius, float outer_radius, SDL_Color color);
static void RAD_ContextMenuItemFillLine(SDL_Renderer *renderer, const RAD_ContextMenuItem_t *item, float x1, float y1, float x2, float y2, float width, SDL_Color color);
static void RAD_ContextMenuItemFillTriangle(SDL_Renderer *renderer, const RAD_ContextMenuItem_t *item, const float points[6], SDL_Color color);
static void RAD_ContextMenuItemDrawIcon(const RAD_ContextMenuItem_t *item, SDL_Renderer *renderer, SDL_Color color);


RAD_ContextMenuItem_t* RAD_CreateContextMenuItem(float angle, int32_t radius)
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
/// Ein schwarzer Kreis, hervorgehoben blau, und darin das Symbol seiner Aktion
/// in Weiss (RAD_ContextMenuItemDrawIcon). Den Blend-Modus setzt die
/// ContextMenuView.
///
void RAD_UpdateContextMenuItem(RAD_ContextMenuItem_t *item, SDL_Renderer *renderer)
{
    const SDL_Color background = item->highlighted ? RAD_CONTEXT_MENU_ITEM_HIGHLIGHT_COLOR : RAD_CONTEXT_MENU_ITEM_BACKGROUND_COLOR;

    RAD_ContextMenuItemFillDisc(renderer, item->center_x, item->center_y, (float)item->radius, background);
    RAD_ContextMenuItemDrawIcon(item, renderer, RAD_CONTEXT_MENU_ITEM_ICON_COLOR);
}


///
/// Zeichnet das Symbol seiner Aktion in "color", in Bruchteilen seines Radius
/// um seinen Mittelpunkt -- (1, 1) ist rechts unten auf dem Rand:
///
///   CLOSE   ein Kreuz
///   DEPLOY  ein Pfeil nach unten auf eine Linie: die Einheit wird abgesetzt
///   MOVE    zwei Winkel nach rechts, ein Doppel-Chevron
///   ATTACK  ein Fadenkreuz
///
/// Ohne Aktion kein Symbol.
///
static void RAD_ContextMenuItemDrawIcon(const RAD_ContextMenuItem_t *item, SDL_Renderer *renderer, SDL_Color color)
{
    const float r = (float)item->radius;

    switch(item->action)
    {
        case RAD_CONTEXT_MENU_ITEM_ACTION_CLOSE:
            RAD_ContextMenuItemFillLine(renderer, item, -0.375f, -0.375f, 0.375f, 0.375f, 0.17f, color);
            RAD_ContextMenuItemFillLine(renderer, item, 0.375f, -0.375f, -0.375f, 0.375f, 0.17f, color);
            break;

        case RAD_CONTEXT_MENU_ITEM_ACTION_DEPLOY:
        {
            const float head[6] = { -0.3f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f };
            RAD_ContextMenuItemFillLine(renderer, item, 0.0f, -0.5f, 0.0f, 0.02f, 0.17f, color);
            RAD_ContextMenuItemFillTriangle(renderer, item, head, color);
            RAD_ContextMenuItemFillLine(renderer, item, -0.46f, 0.46f, 0.46f, 0.46f, 0.125f, color);
            break;
        }

        case RAD_CONTEXT_MENU_ITEM_ACTION_MOVE:
            RAD_ContextMenuItemFillLine(renderer, item, -0.46f, -0.33f, -0.08f, 0.0f, 0.15f, color);
            RAD_ContextMenuItemFillLine(renderer, item, -0.08f, 0.0f, -0.46f, 0.33f, 0.15f, color);
            RAD_ContextMenuItemFillLine(renderer, item, 0.04f, -0.33f, 0.42f, 0.0f, 0.15f, color);
            RAD_ContextMenuItemFillLine(renderer, item, 0.42f, 0.0f, 0.04f, 0.33f, 0.15f, color);
            break;

        case RAD_CONTEXT_MENU_ITEM_ACTION_ATTACK:
            RAD_ContextMenuItemFillRing(renderer, item->center_x, item->center_y, 0.3f * r, 0.4f * r, color);
            RAD_ContextMenuItemFillLine(renderer, item, 0.0f, -0.58f, 0.0f, -0.2f, 0.1f, color);
            RAD_ContextMenuItemFillLine(renderer, item, 0.0f, 0.2f, 0.0f, 0.58f, 0.1f, color);
            RAD_ContextMenuItemFillLine(renderer, item, -0.58f, 0.0f, -0.2f, 0.0f, 0.1f, color);
            RAD_ContextMenuItemFillLine(renderer, item, 0.2f, 0.0f, 0.58f, 0.0f, 0.1f, color);
            RAD_ContextMenuItemFillDisc(renderer, item->center_x, item->center_y, 0.07f * r, color);
            break;

        case RAD_CONTEXT_MENU_ITEM_ACTION_NONE:
            break;
    }
}

///
/// Ein gefuellter Kreis als Faecher aus RAD_CONTEXT_MENU_ITEM_SEGMENTS Dreiecken
/// um den Mittelpunkt, in einem Aufruf von SDL_RenderGeometry.
///
static void RAD_ContextMenuItemFillDisc(SDL_Renderer *renderer, float center_x, float center_y, float radius, SDL_Color color)
{
    // Platz 0 ist der Mittelpunkt, 1 bis SEGMENTS der Rand.
    SDL_Vertex vertices[RAD_CONTEXT_MENU_ITEM_SEGMENTS + 1];
    int indices[3 * RAD_CONTEXT_MENU_ITEM_SEGMENTS];

    vertices[0] = (SDL_Vertex){
        .position = { .x = center_x, .y = center_y },
        .color = color,
        .tex_coord = { .x = 0.0f, .y = 0.0f }
    };
    for(int i = 0; i < RAD_CONTEXT_MENU_ITEM_SEGMENTS; i++)
    {
        const float angle = 2.0f * (float)M_PI * (float)i / (float)RAD_CONTEXT_MENU_ITEM_SEGMENTS;
        vertices[i + 1] = (SDL_Vertex){
            .position = { .x = center_x + radius * cosf(angle), .y = center_y + radius * sinf(angle) },
            .color = color,
            .tex_coord = { .x = 0.0f, .y = 0.0f }
        };

        indices[3 * i] = 0;
        indices[3 * i + 1] = i + 1;
        indices[3 * i + 2] = (i + 1) % RAD_CONTEXT_MENU_ITEM_SEGMENTS + 1;
    }

    SDL_RenderGeometry(renderer, NULL, vertices, RAD_CONTEXT_MENU_ITEM_SEGMENTS + 1, indices, 3 * RAD_CONTEXT_MENU_ITEM_SEGMENTS);
}

///
/// Ein gefuellter Ring zwischen "inner_radius" und "outer_radius" aus
/// RAD_CONTEXT_MENU_ITEM_SEGMENTS Vierecken, je zwei Dreiecke, in einem Aufruf
/// von SDL_RenderGeometry -- wie RAD_ContextMenuViewFillRing.
///
static void RAD_ContextMenuItemFillRing(SDL_Renderer *renderer, float center_x, float center_y, float inner_radius, float outer_radius, SDL_Color color)
{
    // Platz 2i ist der aeussere, 2i + 1 der innere Punkt des i-ten Strahls.
    SDL_Vertex vertices[2 * RAD_CONTEXT_MENU_ITEM_SEGMENTS];
    int indices[6 * RAD_CONTEXT_MENU_ITEM_SEGMENTS];

    for(int i = 0; i < RAD_CONTEXT_MENU_ITEM_SEGMENTS; i++)
    {
        const float angle = 2.0f * (float)M_PI * (float)i / (float)RAD_CONTEXT_MENU_ITEM_SEGMENTS;
        const float cos_angle = cosf(angle);
        const float sin_angle = sinf(angle);
        vertices[2 * i] = (SDL_Vertex){
            .position = { .x = center_x + outer_radius * cos_angle, .y = center_y + outer_radius * sin_angle },
            .color = color,
            .tex_coord = { .x = 0.0f, .y = 0.0f }
        };
        vertices[2 * i + 1] = (SDL_Vertex){
            .position = { .x = center_x + inner_radius * cos_angle, .y = center_y + inner_radius * sin_angle },
            .color = color,
            .tex_coord = { .x = 0.0f, .y = 0.0f }
        };

        // Das Viereck zwischen diesem Strahl und dem naechsten.
        const int outer = 2 * i;
        const int inner = 2 * i + 1;
        const int next_outer = 2 * ((i + 1) % RAD_CONTEXT_MENU_ITEM_SEGMENTS);
        const int next_inner = next_outer + 1;
        indices[6 * i] = outer;
        indices[6 * i + 1] = next_outer;
        indices[6 * i + 2] = inner;
        indices[6 * i + 3] = inner;
        indices[6 * i + 4] = next_outer;
        indices[6 * i + 5] = next_inner;
    }

    SDL_RenderGeometry(renderer, NULL, vertices, 2 * RAD_CONTEXT_MENU_ITEM_SEGMENTS, indices, 6 * RAD_CONTEXT_MENU_ITEM_SEGMENTS);
}

///
/// Ein Strich von (x1, y1) nach (x2, y2), "width" breit, als Viereck aus zwei
/// Dreiecken. Alles in Bruchteilen des Radius um den Mittelpunkt des Eintrags
/// (RAD_ContextMenuItemDrawIcon); die Enden sind gerade abgeschnitten.
///
static void RAD_ContextMenuItemFillLine(SDL_Renderer *renderer, const RAD_ContextMenuItem_t *item, float x1, float y1, float x2, float y2, float width, SDL_Color color)
{
    const float r = (float)item->radius;
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float length = sqrtf(dx * dx + dy * dy);
    if(length <= 0.0f)
    {
        return;
    }

    // Senkrecht zum Strich, halb so lang wie er breit ist.
    const float nx = -dy / length * 0.5f * width;
    const float ny = dx / length * 0.5f * width;

    const SDL_Vertex vertices[4] = {
        { .position = { .x = item->center_x + (x1 + nx) * r, .y = item->center_y + (y1 + ny) * r }, .color = color, .tex_coord = { .x = 0.0f, .y = 0.0f } },
        { .position = { .x = item->center_x + (x2 + nx) * r, .y = item->center_y + (y2 + ny) * r }, .color = color, .tex_coord = { .x = 0.0f, .y = 0.0f } },
        { .position = { .x = item->center_x + (x2 - nx) * r, .y = item->center_y + (y2 - ny) * r }, .color = color, .tex_coord = { .x = 0.0f, .y = 0.0f } },
        { .position = { .x = item->center_x + (x1 - nx) * r, .y = item->center_y + (y1 - ny) * r }, .color = color, .tex_coord = { .x = 0.0f, .y = 0.0f } }
    };
    const int indices[6] = { 0, 1, 2, 0, 2, 3 };

    SDL_RenderGeometry(renderer, NULL, vertices, 4, indices, 6);
}

///
/// Ein gefuelltes Dreieck aus den drei Punkten in "points", je x und y, in
/// Bruchteilen des Radius um den Mittelpunkt des Eintrags
/// (RAD_ContextMenuItemDrawIcon).
///
static void RAD_ContextMenuItemFillTriangle(SDL_Renderer *renderer, const RAD_ContextMenuItem_t *item, const float points[6], SDL_Color color)
{
    const float r = (float)item->radius;
    SDL_Vertex vertices[3];
    for(int i = 0; i < 3; i++)
    {
        vertices[i] = (SDL_Vertex){
            .position = { .x = item->center_x + points[2 * i] * r, .y = item->center_y + points[2 * i + 1] * r },
            .color = color,
            .tex_coord = { .x = 0.0f, .y = 0.0f }
        };
    }

    SDL_RenderGeometry(renderer, NULL, vertices, 3, NULL, 0);
}
