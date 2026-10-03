#include <radish/view/terrain_info_view.h>
#include <radish/view/abbreviations.h>
#include <radish/view/text_view.h>
#include <stdio.h>
#include <stdlib.h>


///
/// Abstand zum Rand und zwischen den Zeilen, in Pixeln.
///
#define RAD_TERRAIN_INFO_VIEW_PADDING 4
#define RAD_TERRAIN_INFO_VIEW_GAP 4

///
/// Platz fuer einen Wert als Text samt abschliessender Null: "MV -2147483648".
///
#define RAD_TERRAIN_INFO_VIEW_VALUE_MAX 16


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_TerrainInfoView
{
    ///
    /// Die Breite, die sie hoechstens belegt (RAD_CreateTerrainInfoView), und die
    /// Flaeche, die sie gerade belegt: so breit wie ihr breitester Text samt
    /// Rand (RAD_TerrainInfoViewFitWidth).
    ///
    int32_t max_width;
    SDL_Rect area;
    RAD_TextView_t *name;
    RAD_TextView_t *movement_modifier;
    RAD_TextView_t *cover;
};


static void RAD_TerrainInfoViewFitWidth(RAD_TerrainInfoView_t *terrain_info_view);


RAD_TerrainInfoView_t* RAD_CreateTerrainInfoView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font)
{
    RAD_TerrainInfoView_t *terrain_info_view = malloc(sizeof(struct RAD_TerrainInfoView));
    if(terrain_info_view == NULL)
    {
        return NULL;
    }

    // So hoch wie der Inhalt -- drei Zeilen samt Rand --, hoechstens "height",
    // und an die Unterkante der Flaeche. Der Inhalt aendert seine Hoehe nicht,
    // anders als bei der UnitInfoView reicht es also, das hier einmal zu tun.
    const int32_t line = TTF_FontLineSkip(font);
    const int32_t step = line + RAD_TERRAIN_INFO_VIEW_GAP;
    int32_t content_height = 2 * RAD_TERRAIN_INFO_VIEW_PADDING + 3 * line + 2 * RAD_TERRAIN_INFO_VIEW_GAP;
    if(content_height > height)
    {
        content_height = height;
    }
    const int32_t top = y + height - content_height;

    const int32_t text_x = x + RAD_TERRAIN_INFO_VIEW_PADDING;
    const int32_t text_width = width - 2 * RAD_TERRAIN_INFO_VIEW_PADDING;
    const int32_t first_top = top + RAD_TERRAIN_INFO_VIEW_PADDING;

    // Die TextViews bekommen die volle Breite; was die TerrainInfoView schmaler
    // ist, schneidet RAD_UpdateTerrainInfoView ab.
    *terrain_info_view = (struct RAD_TerrainInfoView){
        .max_width = width,
        .area = { .x = x, .y = top, .w = width, .h = content_height },
        .name = RAD_CreateTextView(text_x, first_top, text_width, line, font),
        .movement_modifier = RAD_CreateTextView(text_x, first_top + step, text_width, line, font),
        .cover = RAD_CreateTextView(text_x, first_top + 2 * step, text_width, line, font)
    };

    if(terrain_info_view->name == NULL || terrain_info_view->movement_modifier == NULL || terrain_info_view->cover == NULL
       || !RAD_TerrainInfoViewSetMovementModifier(terrain_info_view, 0)
       || !RAD_TerrainInfoViewSetCover(terrain_info_view, 0))
    {
        RAD_DestroyTerrainInfoView(&terrain_info_view);
        return NULL;
    }

    return terrain_info_view;
}

///
/// Baut auch eine halb angelegte TerrainInfoView ab (RAD_CreateTerrainInfoView
/// im Fehlerfall): einzelne TextViews sind dann womoeglich NULL.
///
void RAD_DestroyTerrainInfoView(RAD_TerrainInfoView_t **terrain_info_view)
{
    RAD_TerrainInfoView_t *v = *terrain_info_view;

    if(v->name != NULL)
    {
        RAD_DestroyTextView(&v->name);
    }
    if(v->movement_modifier != NULL)
    {
        RAD_DestroyTextView(&v->movement_modifier);
    }
    if(v->cover != NULL)
    {
        RAD_DestroyTextView(&v->cover);
    }

    free(v);
    *terrain_info_view = NULL;
}

bool RAD_TerrainInfoViewSetName(RAD_TerrainInfoView_t *terrain_info_view, const char *name)
{
    const bool set = RAD_TextViewSetText(terrain_info_view->name, name);
    RAD_TerrainInfoViewFitWidth(terrain_info_view);
    return set;
}

bool RAD_TerrainInfoViewSetMovementModifier(RAD_TerrainInfoView_t *terrain_info_view, int32_t movement_modifier)
{
    char text[RAD_TERRAIN_INFO_VIEW_VALUE_MAX];
    snprintf(text, sizeof(text), "%s %+d", RAD_VIEW_ABBREVIATION_MOVEMENT_RANGE, (int)movement_modifier);
    const bool set = RAD_TextViewSetText(terrain_info_view->movement_modifier, text);
    RAD_TerrainInfoViewFitWidth(terrain_info_view);
    return set;
}

bool RAD_TerrainInfoViewSetCover(RAD_TerrainInfoView_t *terrain_info_view, int32_t cover)
{
    char text[RAD_TERRAIN_INFO_VIEW_VALUE_MAX];
    snprintf(text, sizeof(text), "%s %d", RAD_VIEW_ABBREVIATION_COVER, (int)cover);
    const bool set = RAD_TextViewSetText(terrain_info_view->cover, text);
    RAD_TerrainInfoViewFitWidth(terrain_info_view);
    return set;
}

void RAD_UpdateTerrainInfoView(RAD_TerrainInfoView_t *terrain_info_view, SDL_Renderer *renderer)
{
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderFillRect(renderer, &terrain_info_view->area);

    // Ist die Flaeche kleiner als der Inhalt, schneidet sie ihn ab; danach die
    // Clip-Flaeche des Aufrufers wiederherstellen.
    const SDL_bool was_clipping = SDL_RenderIsClipEnabled(renderer);
    SDL_Rect previous_clip;
    SDL_RenderGetClipRect(renderer, &previous_clip);
    SDL_RenderSetClipRect(renderer, &terrain_info_view->area);

    RAD_UpdateTextView(terrain_info_view->name, renderer);
    RAD_UpdateTextView(terrain_info_view->movement_modifier, renderer);
    RAD_UpdateTextView(terrain_info_view->cover, renderer);

    SDL_RenderSetClipRect(renderer, was_clipping ? &previous_clip : NULL);
}


///
/// Passt die Breite von "area" dem breitesten Text an, samt Rand, hoechstens
/// "max_width"; die linke Kante bleibt. Nach jedem Setter zu rufen -- die
/// Texte aendern sich nur dort. Waehrend RAD_CreateTerrainInfoView sind noch
/// nicht alle TextViews angelegt; dann geschieht nichts.
///
static void RAD_TerrainInfoViewFitWidth(RAD_TerrainInfoView_t *terrain_info_view)
{
    if(terrain_info_view->name == NULL || terrain_info_view->movement_modifier == NULL || terrain_info_view->cover == NULL)
    {
        return;
    }

    int32_t widest = RAD_TextViewTextWidth(terrain_info_view->name);
    const int32_t movement_modifier = RAD_TextViewTextWidth(terrain_info_view->movement_modifier);
    const int32_t cover = RAD_TextViewTextWidth(terrain_info_view->cover);
    widest = (movement_modifier > widest) ? movement_modifier : widest;
    widest = (cover > widest) ? cover : widest;

    const int32_t width = widest + 2 * RAD_TERRAIN_INFO_VIEW_PADDING;
    terrain_info_view->area.w = (width < terrain_info_view->max_width) ? width : terrain_info_view->max_width;
}
