#include <radish/view/terrain_info_view.h>
#include <radish/control/terrain.h>
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
/// Steht an Stelle eines Wertes, wenn nichts im Fokus steht.
///
#define RAD_TERRAIN_INFO_VIEW_NO_VALUE "-"


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

    ///
    /// Was gerade im Fokus steht (model/focus.h). Gehoert dem Aufrufer.
    ///
    const RAD_ClientFocus_t *focus;

    ///
    /// Wozu Name, MV und CV gerade angezeigt werden (RAD_TerrainInfoViewSyncFocus):
    /// das Feld, NULL fuer "-", und was control/terrain.h dazu sagt. Gilt erst,
    /// wenn "synced".
    ///
    bool synced;
    const RAD_ClientTile_t *shown_tile;
    const char *shown_name;
    int32_t shown_movement_modifier;
    int32_t shown_cover;
};


static void RAD_TerrainInfoViewFitWidth(RAD_TerrainInfoView_t *terrain_info_view);
static bool RAD_TerrainInfoViewSyncFocus(RAD_TerrainInfoView_t *terrain_info_view);
static bool RAD_TerrainInfoViewSetValue(RAD_TerrainInfoView_t *terrain_info_view, RAD_TextView_t *text_view, const char *abbreviation, const char *value);


RAD_TerrainInfoView_t* RAD_CreateTerrainInfoView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font, const RAD_ClientFocus_t *focus)
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
        .cover = RAD_CreateTextView(text_x, first_top + 2 * step, text_width, line, font),
        .focus = focus,
        .synced = false,
        .shown_tile = NULL,
        .shown_name = NULL,
        .shown_movement_modifier = 0,
        .shown_cover = 0
    };

    if(terrain_info_view->name == NULL || terrain_info_view->movement_modifier == NULL || terrain_info_view->cover == NULL
       || !RAD_TerrainInfoViewSyncFocus(terrain_info_view))
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

void RAD_UpdateTerrainInfoView(RAD_TerrainInfoView_t *terrain_info_view, SDL_Renderer *renderer)
{
    // Scheitert es, bleibt der alte Text stehen; der naechste Frame versucht es neu.
    RAD_TerrainInfoViewSyncFocus(terrain_info_view);

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

///
/// Setzt Name, MV und CV auf das Feld im Fokus (control/terrain.h), bzw. auf "-",
/// wenn nichts im Fokus steht -- nur, wenn sich seit dem letzten Mal etwas
/// geaendert hat: jeder neue Text muss neu gerendert werden. false, wenn fuer
/// einen Text kein Speicher da ist; dann gilt sie als nicht auf Stand.
///
static bool RAD_TerrainInfoViewSyncFocus(RAD_TerrainInfoView_t *terrain_info_view)
{
    const RAD_ClientTile_t *tile = terrain_info_view->focus->tile;
    // Statische Texte aus control/terrain.c: der Zeiger reicht zum Vergleichen.
    const char *name = (tile != NULL) ? RAD_ControlTerrainName(tile) : RAD_TERRAIN_INFO_VIEW_NO_VALUE;
    const int32_t movement_modifier = (tile != NULL) ? RAD_ControlTerrainMovementModifier(tile) : 0;
    const int32_t cover = (tile != NULL) ? RAD_ControlTerrainCover(tile) : 0;

    if(terrain_info_view->synced
       && tile == terrain_info_view->shown_tile
       && name == terrain_info_view->shown_name
       && movement_modifier == terrain_info_view->shown_movement_modifier
       && cover == terrain_info_view->shown_cover)
    {
        return true;
    }

    char movement_modifier_text[RAD_TERRAIN_INFO_VIEW_VALUE_MAX] = RAD_TERRAIN_INFO_VIEW_NO_VALUE;
    char cover_text[RAD_TERRAIN_INFO_VIEW_VALUE_MAX] = RAD_TERRAIN_INFO_VIEW_NO_VALUE;
    if(tile != NULL)
    {
        snprintf(movement_modifier_text, sizeof(movement_modifier_text), "%+d", (int)movement_modifier);
        snprintf(cover_text, sizeof(cover_text), "%d", (int)cover);
    }

    terrain_info_view->synced =
        RAD_TextViewSetText(terrain_info_view->name, name)
        && RAD_TerrainInfoViewSetValue(terrain_info_view, terrain_info_view->movement_modifier, RAD_VIEW_ABBREVIATION_MOVEMENT_RANGE, movement_modifier_text)
        && RAD_TerrainInfoViewSetValue(terrain_info_view, terrain_info_view->cover, RAD_VIEW_ABBREVIATION_COVER, cover_text);
    terrain_info_view->shown_tile = tile;
    terrain_info_view->shown_name = name;
    terrain_info_view->shown_movement_modifier = movement_modifier;
    terrain_info_view->shown_cover = cover;
    return terrain_info_view->synced;
}

///
/// Setzt "text_view" auf "<abbreviation> <value>", z.B. "MV +1" oder "CV -".
/// false, wenn kein Speicher da ist (RAD_TextViewSetText).
///
static bool RAD_TerrainInfoViewSetValue(RAD_TerrainInfoView_t *terrain_info_view, RAD_TextView_t *text_view, const char *abbreviation, const char *value)
{
    char text[RAD_TERRAIN_INFO_VIEW_VALUE_MAX];
    snprintf(text, sizeof(text), "%s %s", abbreviation, value);
    const bool set = RAD_TextViewSetText(text_view, text);
    RAD_TerrainInfoViewFitWidth(terrain_info_view);
    return set;
}
