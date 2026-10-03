#include <radish/view/weapon_view.h>
#include <radish/view/abbreviations.h>
#include <radish/view/labeled_bar_view.h>
#include <stdlib.h>


///
/// Die Attribute in der Reihenfolge, in der sie stehen: zeilenweise, je Zeile
/// RAD_WEAPON_VIEW_COLUMNS nebeneinander (view/weapon_view.h).
///
typedef enum
{
    RAD_WEAPON_VIEW_STRENGTH = 0,
    RAD_WEAPON_VIEW_PENETRATION,
    RAD_WEAPON_VIEW_RANGE,
    RAD_WEAPON_VIEW_NUMBER_OF_SHOTS,
    RAD_WEAPON_VIEW_ATTRIBUTES
} RAD_WeaponViewAttribute_t;

static const char *RAD_WEAPON_VIEW_LABELS[RAD_WEAPON_VIEW_ATTRIBUTES] = {
    [RAD_WEAPON_VIEW_STRENGTH] = RAD_VIEW_ABBREVIATION_STRENGTH,
    [RAD_WEAPON_VIEW_PENETRATION] = RAD_VIEW_ABBREVIATION_PENETRATION,
    [RAD_WEAPON_VIEW_RANGE] = RAD_VIEW_ABBREVIATION_RANGE,
    [RAD_WEAPON_VIEW_NUMBER_OF_SHOTS] = RAD_VIEW_ABBREVIATION_NUMBER_OF_SHOTS
};

#define RAD_WEAPON_VIEW_COLUMNS 2
#define RAD_WEAPON_VIEW_ROWS 2

///
/// Abstand zwischen den LabeledBarViews, waagrecht und senkrecht, in Pixeln.
///
#define RAD_WEAPON_VIEW_GAP 4


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_WeaponView
{
    SDL_Rect area;
    RAD_LabeledBarView_t *attributes[RAD_WEAPON_VIEW_ATTRIBUTES];
};


RAD_WeaponView_t* RAD_CreateWeaponView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font)
{
    RAD_WeaponView_t *weapon_view = malloc(sizeof(struct RAD_WeaponView));
    if(weapon_view == NULL)
    {
        return NULL;
    }

    *weapon_view = (struct RAD_WeaponView){
        .area = { .x = x, .y = y, .w = width, .h = height },
        .attributes = { NULL }
    };

    const int32_t cell_width = (width - (RAD_WEAPON_VIEW_COLUMNS - 1) * RAD_WEAPON_VIEW_GAP) / RAD_WEAPON_VIEW_COLUMNS;
    const int32_t cell_height = (height - (RAD_WEAPON_VIEW_ROWS - 1) * RAD_WEAPON_VIEW_GAP) / RAD_WEAPON_VIEW_ROWS;

    for(int i = 0; i < RAD_WEAPON_VIEW_ATTRIBUTES; ++i)
    {
        const int32_t column = i % RAD_WEAPON_VIEW_COLUMNS;
        const int32_t row = i / RAD_WEAPON_VIEW_COLUMNS;

        weapon_view->attributes[i] = RAD_CreateLabeledBarView(
            x + column * (cell_width + RAD_WEAPON_VIEW_GAP),
            y + row * (cell_height + RAD_WEAPON_VIEW_GAP),
            cell_width,
            cell_height,
            font
        );

        if(weapon_view->attributes[i] == NULL || !RAD_LabeledBarViewSetLabel(weapon_view->attributes[i], RAD_WEAPON_VIEW_LABELS[i]))
        {
            RAD_DestroyWeaponView(&weapon_view);
            return NULL;
        }
    }

    return weapon_view;
}

///
/// Baut auch eine halb angelegte WeaponView ab (RAD_CreateWeaponView im
/// Fehlerfall): einzelne LabeledBarViews sind dann womoeglich NULL.
///
void RAD_DestroyWeaponView(RAD_WeaponView_t **weapon_view)
{
    RAD_WeaponView_t *v = *weapon_view;

    for(int i = 0; i < RAD_WEAPON_VIEW_ATTRIBUTES; ++i)
    {
        if(v->attributes[i] != NULL)
        {
            RAD_DestroyLabeledBarView(&v->attributes[i]);
        }
    }

    free(v);
    *weapon_view = NULL;
}

void RAD_WeaponViewSetStrength(RAD_WeaponView_t *weapon_view, int32_t current, int32_t maximum)
{
    RAD_LabeledBarViewSetValue(weapon_view->attributes[RAD_WEAPON_VIEW_STRENGTH], current, maximum);
}

void RAD_WeaponViewSetPenetration(RAD_WeaponView_t *weapon_view, int32_t current, int32_t maximum)
{
    RAD_LabeledBarViewSetValue(weapon_view->attributes[RAD_WEAPON_VIEW_PENETRATION], current, maximum);
}

void RAD_WeaponViewSetRange(RAD_WeaponView_t *weapon_view, int32_t current, int32_t maximum)
{
    RAD_LabeledBarViewSetValue(weapon_view->attributes[RAD_WEAPON_VIEW_RANGE], current, maximum);
}

void RAD_WeaponViewSetNumberOfShots(RAD_WeaponView_t *weapon_view, int32_t current, int32_t maximum)
{
    RAD_LabeledBarViewSetValue(weapon_view->attributes[RAD_WEAPON_VIEW_NUMBER_OF_SHOTS], current, maximum);
}

void RAD_UpdateWeaponView(RAD_WeaponView_t *weapon_view, SDL_Renderer *renderer)
{
    for(int i = 0; i < RAD_WEAPON_VIEW_ATTRIBUTES; ++i)
    {
        RAD_UpdateLabeledBarView(weapon_view->attributes[i], renderer);
    }
}
