#include <radish/view/entity_view.h>
#include <radish/view/abbreviations.h>
#include <radish/view/labeled_bar_view.h>
#include <radish/view/text_view.h>
#include <radish/view/weapon_view.h>
#include <stdlib.h>


///
/// Abstand zwischen den Zeilen, in Pixeln (view/entity_view.h).
///
#define RAD_ENTITY_VIEW_GAP 4

///
/// Hoehe der Attribute der Entitaet und der WeaponView einer Waffe: beide haben
/// zwei Zeilen (view/entity_view.h, view/weapon_view.h).
///
#define RAD_ENTITY_VIEW_ATTRIBUTES_HEIGHT(line) (2 * (line) + RAD_ENTITY_VIEW_GAP)

///
/// Die Attribute der Entitaet in der Reihenfolge, in der sie stehen: zeilenweise,
/// je Zeile RAD_ENTITY_VIEW_COLUMNS nebeneinander (view/entity_view.h).
///
typedef enum
{
    RAD_ENTITY_VIEW_HIT_POINTS = 0,
    RAD_ENTITY_VIEW_ARMOR,
    RAD_ENTITY_VIEW_STRENGTH,
    RAD_ENTITY_VIEW_RESISTANCE,
    RAD_ENTITY_VIEW_ATTRIBUTES
} RAD_EntityViewAttribute_t;

static const char *RAD_ENTITY_VIEW_LABELS[RAD_ENTITY_VIEW_ATTRIBUTES] = {
    [RAD_ENTITY_VIEW_HIT_POINTS] = RAD_VIEW_ABBREVIATION_HIT_POINTS,
    [RAD_ENTITY_VIEW_ARMOR] = RAD_VIEW_ABBREVIATION_ARMOR,
    [RAD_ENTITY_VIEW_STRENGTH] = RAD_VIEW_ABBREVIATION_STRENGTH,
    [RAD_ENTITY_VIEW_RESISTANCE] = RAD_VIEW_ABBREVIATION_RESISTANCE
};

#define RAD_ENTITY_VIEW_COLUMNS 2


///
/// Eine Waffe: ihr Name und darunter ihre Attribute. "bottom" ist die
/// Unterkante in Pixeln -- liegt sie unter der der EntityView, wird die Waffe
/// nicht gezeichnet.
///
typedef struct
{
    RAD_TextView_t *name;
    RAD_WeaponView_t *attributes;
    int32_t bottom;
} RAD_EntityViewWeapon_t;

///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_EntityView
{
    SDL_Rect area;
    RAD_TextView_t *name;
    RAD_LabeledBarView_t *attributes[RAD_ENTITY_VIEW_ATTRIBUTES];

    ///
    /// Alle RAD_ENTITY_VIEW_WEAPONS_MAX Waffen sind von Anfang an angelegt;
    /// gezeichnet werden nur die ersten number_of_weapons.
    ///
    size_t number_of_weapons;
    RAD_EntityViewWeapon_t weapons[RAD_ENTITY_VIEW_WEAPONS_MAX];
};


static void RAD_EntityViewResetWeapon(RAD_EntityViewWeapon_t *weapon);


RAD_EntityView_t* RAD_CreateEntityView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font)
{
    RAD_EntityView_t *entity_view = malloc(sizeof(struct RAD_EntityView));
    if(entity_view == NULL)
    {
        return NULL;
    }

    *entity_view = (struct RAD_EntityView){
        .area = { .x = x, .y = y, .w = width, .h = height },
        .name = NULL,
        .attributes = { NULL },
        .number_of_weapons = 0,
        .weapons = { { NULL, NULL, 0 } }
    };

    const int32_t line = TTF_FontLineSkip(font);
    int32_t top = y;

    entity_view->name = RAD_CreateTextView(x, top, width, line, font);
    top += line + RAD_ENTITY_VIEW_GAP;

    if(entity_view->name == NULL)
    {
        RAD_DestroyEntityView(&entity_view);
        return NULL;
    }

    // Die Attribute in zwei Zeilen zu je zwei, wie in der WeaponView.
    const int32_t attributes_height = RAD_ENTITY_VIEW_ATTRIBUTES_HEIGHT(line);
    const int32_t cell_width = (width - (RAD_ENTITY_VIEW_COLUMNS - 1) * RAD_ENTITY_VIEW_GAP) / RAD_ENTITY_VIEW_COLUMNS;

    for(int i = 0; i < RAD_ENTITY_VIEW_ATTRIBUTES; ++i)
    {
        const int32_t column = i % RAD_ENTITY_VIEW_COLUMNS;
        const int32_t row = i / RAD_ENTITY_VIEW_COLUMNS;

        entity_view->attributes[i] = RAD_CreateLabeledBarView(
            x + column * (cell_width + RAD_ENTITY_VIEW_GAP),
            top + row * (line + RAD_ENTITY_VIEW_GAP),
            cell_width,
            line,
            font
        );

        if(entity_view->attributes[i] == NULL || !RAD_LabeledBarViewSetLabel(entity_view->attributes[i], RAD_ENTITY_VIEW_LABELS[i]))
        {
            RAD_DestroyEntityView(&entity_view);
            return NULL;
        }
    }
    top += attributes_height + RAD_ENTITY_VIEW_GAP;

    for(size_t i = 0; i < RAD_ENTITY_VIEW_WEAPONS_MAX; ++i)
    {
        RAD_EntityViewWeapon_t *weapon = &entity_view->weapons[i];

        weapon->name = RAD_CreateTextView(x, top, width, line, font);
        top += line + RAD_ENTITY_VIEW_GAP;

        weapon->attributes = RAD_CreateWeaponView(x, top, width, attributes_height, font);
        top += attributes_height;

        weapon->bottom = top;
        top += RAD_ENTITY_VIEW_GAP;

        if(weapon->name == NULL || weapon->attributes == NULL)
        {
            RAD_DestroyEntityView(&entity_view);
            return NULL;
        }
    }

    return entity_view;
}

///
/// Baut auch eine halb angelegte EntityView ab (RAD_CreateEntityView im
/// Fehlerfall): einzelne Kinder sind dann womoeglich NULL.
///
void RAD_DestroyEntityView(RAD_EntityView_t **entity_view)
{
    RAD_EntityView_t *v = *entity_view;

    for(size_t i = 0; i < RAD_ENTITY_VIEW_WEAPONS_MAX; ++i)
    {
        if(v->weapons[i].name != NULL)
        {
            RAD_DestroyTextView(&v->weapons[i].name);
        }
        if(v->weapons[i].attributes != NULL)
        {
            RAD_DestroyWeaponView(&v->weapons[i].attributes);
        }
    }
    for(int i = 0; i < RAD_ENTITY_VIEW_ATTRIBUTES; ++i)
    {
        if(v->attributes[i] != NULL)
        {
            RAD_DestroyLabeledBarView(&v->attributes[i]);
        }
    }
    if(v->name != NULL)
    {
        RAD_DestroyTextView(&v->name);
    }

    free(v);
    *entity_view = NULL;
}

int32_t RAD_EntityViewHeight(TTF_Font *font, size_t number_of_weapons)
{
    if(number_of_weapons > RAD_ENTITY_VIEW_WEAPONS_MAX)
    {
        number_of_weapons = RAD_ENTITY_VIEW_WEAPONS_MAX;
    }

    // Wie in RAD_CreateEntityView: Name und Attribute, dann je Waffe ihr Name und
    // ihre WeaponView, alles durch RAD_ENTITY_VIEW_GAP getrennt.
    const int32_t line = TTF_FontLineSkip(font);
    int32_t height = line + RAD_ENTITY_VIEW_GAP + RAD_ENTITY_VIEW_ATTRIBUTES_HEIGHT(line);
    for(size_t i = 0; i < number_of_weapons; ++i)
    {
        height += RAD_ENTITY_VIEW_GAP + line + RAD_ENTITY_VIEW_GAP + RAD_ENTITY_VIEW_ATTRIBUTES_HEIGHT(line);
    }
    return height;
}

bool RAD_EntityViewSetName(RAD_EntityView_t *entity_view, const char *name)
{
    return RAD_TextViewSetText(entity_view->name, name);
}

void RAD_EntityViewSetHitPoints(RAD_EntityView_t *entity_view, int32_t current, int32_t maximum)
{
    RAD_LabeledBarViewSetValue(entity_view->attributes[RAD_ENTITY_VIEW_HIT_POINTS], current, maximum);
}

void RAD_EntityViewSetArmor(RAD_EntityView_t *entity_view, int32_t current, int32_t maximum)
{
    RAD_LabeledBarViewSetValue(entity_view->attributes[RAD_ENTITY_VIEW_ARMOR], current, maximum);
}

void RAD_EntityViewSetStrength(RAD_EntityView_t *entity_view, int32_t current, int32_t maximum)
{
    RAD_LabeledBarViewSetValue(entity_view->attributes[RAD_ENTITY_VIEW_STRENGTH], current, maximum);
}

void RAD_EntityViewSetResistance(RAD_EntityView_t *entity_view, int32_t current, int32_t maximum)
{
    RAD_LabeledBarViewSetValue(entity_view->attributes[RAD_ENTITY_VIEW_RESISTANCE], current, maximum);
}

bool RAD_EntityViewSetNumberOfWeapons(RAD_EntityView_t *entity_view, size_t number_of_weapons)
{
    if(number_of_weapons > RAD_ENTITY_VIEW_WEAPONS_MAX)
    {
        return false;
    }

    for(size_t i = entity_view->number_of_weapons; i < number_of_weapons; ++i)
    {
        RAD_EntityViewResetWeapon(&entity_view->weapons[i]);
    }

    entity_view->number_of_weapons = number_of_weapons;
    return true;
}

bool RAD_EntityViewSetWeaponName(RAD_EntityView_t *entity_view, size_t weapon, const char *name)
{
    if(weapon >= entity_view->number_of_weapons)
    {
        return false;
    }
    return RAD_TextViewSetText(entity_view->weapons[weapon].name, name);
}

bool RAD_EntityViewSetWeaponStrength(RAD_EntityView_t *entity_view, size_t weapon, int32_t current, int32_t maximum)
{
    if(weapon >= entity_view->number_of_weapons)
    {
        return false;
    }
    RAD_WeaponViewSetStrength(entity_view->weapons[weapon].attributes, current, maximum);
    return true;
}

bool RAD_EntityViewSetWeaponPenetration(RAD_EntityView_t *entity_view, size_t weapon, int32_t current, int32_t maximum)
{
    if(weapon >= entity_view->number_of_weapons)
    {
        return false;
    }
    RAD_WeaponViewSetPenetration(entity_view->weapons[weapon].attributes, current, maximum);
    return true;
}

bool RAD_EntityViewSetWeaponRange(RAD_EntityView_t *entity_view, size_t weapon, int32_t current, int32_t maximum)
{
    if(weapon >= entity_view->number_of_weapons)
    {
        return false;
    }
    RAD_WeaponViewSetRange(entity_view->weapons[weapon].attributes, current, maximum);
    return true;
}

bool RAD_EntityViewSetWeaponNumberOfShots(RAD_EntityView_t *entity_view, size_t weapon, int32_t current, int32_t maximum)
{
    if(weapon >= entity_view->number_of_weapons)
    {
        return false;
    }
    RAD_WeaponViewSetNumberOfShots(entity_view->weapons[weapon].attributes, current, maximum);
    return true;
}

void RAD_UpdateEntityView(RAD_EntityView_t *entity_view, SDL_Renderer *renderer)
{
    RAD_UpdateTextView(entity_view->name, renderer);
    for(int i = 0; i < RAD_ENTITY_VIEW_ATTRIBUTES; ++i)
    {
        RAD_UpdateLabeledBarView(entity_view->attributes[i], renderer);
    }

    const int32_t bottom = entity_view->area.y + entity_view->area.h;
    for(size_t i = 0; i < entity_view->number_of_weapons; ++i)
    {
        const RAD_EntityViewWeapon_t *weapon = &entity_view->weapons[i];
        if(weapon->bottom > bottom)
        {
            break;
        }

        RAD_UpdateTextView(weapon->name, renderer);
        RAD_UpdateWeaponView(weapon->attributes, renderer);
    }
}


///
/// Bringt eine Waffe in den Zustand einer neuen: leerer Name, alle Werte 0 von 0.
///
static void RAD_EntityViewResetWeapon(RAD_EntityViewWeapon_t *weapon)
{
    // Ein leerer Text braucht keinen Speicher, das kann nicht scheitern.
    RAD_TextViewSetText(weapon->name, NULL);
    RAD_WeaponViewSetStrength(weapon->attributes, 0, 0);
    RAD_WeaponViewSetPenetration(weapon->attributes, 0, 0);
    RAD_WeaponViewSetRange(weapon->attributes, 0, 0);
    RAD_WeaponViewSetNumberOfShots(weapon->attributes, 0, 0);
}
