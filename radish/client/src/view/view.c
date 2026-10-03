#include <radish/view/view.h>
#include <radish/view/iso_map_view.h>
#include <radish/view/terrain_info_view.h>
#include <radish/view/unit_info_view.h>
#include <radish/io/camera_control.h>
#include <stdio.h>
#include <stdlib.h>


///
/// Groesse von TerrainInfoView und UnitInfoView (view/view.h): je so breit, in
/// Pixeln, und hoechstens so hoch, in Prozent der Zeichenflaeche -- beide passen
/// sich ihrem Inhalt an.
///
#define RAD_VIEW_INFO_WIDTH 200
#define RAD_VIEW_TERRAIN_INFO_MAX_HEIGHT_PERCENT 27
#define RAD_VIEW_UNIT_INFO_MAX_HEIGHT_PERCENT 100


///
/// Die Struktur steht hier und nicht im Header: an Fenster, Renderer und Schrift
/// soll niemand vorbei an RAD_CreateView/RAD_DestroyView herankommen.
///
struct RAD_RootView
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    RAD_IsoMapView_t *iso_map_view;
    RAD_TerrainInfoView_t *terrain_info_view;
    RAD_UnitInfoView_t *unit_info_view;
    bool terrain_info_visible;
    bool unit_info_visible;

    ///
    /// Haelt keine eigenen Ressourcen (io/camera_control.h) -- abzuraeumen ist
    /// sie mit der RootView selbst.
    ///
    RAD_IoCameraControl_t camera_control;

    ///
    /// Gehoeren dem Aufrufer (view/view.h, RAD_CreateView).
    ///
    RAD_IsoMap_t *map;
    RAD_IoUserInput_t *user_input;
};


static void RAD_ViewHandleEvent(RAD_RootView_t *view, const SDL_Event *event);
static void RAD_ViewAddExampleUnit(RAD_UnitInfoView_t *unit_info_view);
static void RAD_ViewAddExampleTerrain(RAD_TerrainInfoView_t *terrain_info_view);


RAD_RootView_t* RAD_CreateView(
    const char *title,
    int32_t width,
    int32_t height,
    const char *font_path,
    int32_t font_size,
    RAD_IsoMap_t *map,
    RAD_IoUserInput_t *user_input,
    const RAD_ClientWorld_t *world
)
{
    RAD_RootView_t *view = malloc(sizeof(struct RAD_RootView));
    if(view == NULL)
    {
        return NULL;
    }

    *view = (struct RAD_RootView){
        .window = NULL,
        .renderer = NULL,
        .font = NULL,
        .iso_map_view = NULL,
        .terrain_info_view = NULL,
        .unit_info_view = NULL,
        .terrain_info_visible = true,
        .unit_info_visible = true,
        .map = map,
        .user_input = user_input
    };

    if(SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL_Init: %s\n", SDL_GetError());
        free(view);
        return NULL;
    }

    if(TTF_Init() != 0)
    {
        printf("TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        free(view);
        return NULL;
    }

    view->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    width, height, SDL_WINDOW_SHOWN);
    if(view->window == NULL)
    {
        printf("SDL_CreateWindow: %s\n", SDL_GetError());
        RAD_DestroyView(&view);
        return NULL;
    }

    view->renderer = SDL_CreateRenderer(view->window, -1, SDL_RENDERER_ACCELERATED);
    if(view->renderer == NULL)
    {
        printf("SDL_CreateRenderer: %s\n", SDL_GetError());
        RAD_DestroyView(&view);
        return NULL;
    }

    view->font = TTF_OpenFont(font_path, font_size);
    if(view->font == NULL)
    {
        printf("TTF_OpenFont %s: %s\n", font_path, TTF_GetError());
        RAD_DestroyView(&view);
        return NULL;
    }

    int drawable_width = 0, drawable_height = 0;
    SDL_GetWindowSizeInPixels(view->window, &drawable_width, &drawable_height);

    view->camera_control = RAD_CreateIoCameraControl(map, drawable_width, drawable_height);

    view->iso_map_view = RAD_CreateIsoMapView(0, 0, drawable_width, drawable_height, map, world);
    if(view->iso_map_view == NULL)
    {
        printf("Keine IsoMapView\n");
        RAD_DestroyView(&view);
        return NULL;
    }

    // Beide am unteren Rand, die TerrainInfoView am linken, die UnitInfoView am
    // rechten. Ist die Zeichenflaeche schmaler als beide zusammen, teilen sie sich
    // ihre Breite, damit sie sich nicht ueberlappen.
    const int32_t info_width = (drawable_width >= 2 * RAD_VIEW_INFO_WIDTH) ? RAD_VIEW_INFO_WIDTH : drawable_width / 2;
    const int32_t terrain_info_max_height = (drawable_height * RAD_VIEW_TERRAIN_INFO_MAX_HEIGHT_PERCENT) / 100;

    view->terrain_info_view = RAD_CreateTerrainInfoView(
        0,
        drawable_height - terrain_info_max_height,
        info_width,
        terrain_info_max_height,
        view->font
    );
    const int32_t unit_info_max_height = (drawable_height * RAD_VIEW_UNIT_INFO_MAX_HEIGHT_PERCENT) / 100;
    view->unit_info_view = RAD_CreateUnitInfoView(
        drawable_width - info_width,
        drawable_height - unit_info_max_height,
        info_width,
        unit_info_max_height,
        view->font
    );
    if(view->terrain_info_view == NULL || view->unit_info_view == NULL)
    {
        printf("Keine TerrainInfoView oder UnitInfoView\n");
        RAD_DestroyView(&view);
        return NULL;
    }

    RAD_ViewAddExampleUnit(view->unit_info_view);
    RAD_ViewAddExampleTerrain(view->terrain_info_view);

    SDL_StartTextInput();

    return view;
}

///
/// Baut auch eine halb angelegte Komponente ab (RAD_CreateView im Fehlerfall):
/// SDL und SDL_ttf sind dann schon initialisiert, Fenster, Renderer, Schrift
/// oder eine der Views womoeglich noch NULL.
///
void RAD_DestroyView(RAD_RootView_t **view)
{
    RAD_RootView_t *v = *view;

    SDL_StopTextInput();

    if(v->unit_info_view != NULL)
    {
        RAD_DestroyUnitInfoView(&v->unit_info_view);
    }
    if(v->terrain_info_view != NULL)
    {
        RAD_DestroyTerrainInfoView(&v->terrain_info_view);
    }
    if(v->iso_map_view != NULL)
    {
        RAD_DestroyIsoMapView(&v->iso_map_view);
    }
    if(v->font != NULL)
    {
        TTF_CloseFont(v->font);
    }
    if(v->renderer != NULL)
    {
        SDL_DestroyRenderer(v->renderer);
    }
    if(v->window != NULL)
    {
        SDL_DestroyWindow(v->window);
    }

    TTF_Quit();
    SDL_Quit();

    free(v);
    *view = NULL;
}

void RAD_UpdateView(RAD_RootView_t *view)
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        RAD_ViewHandleEvent(view, &event);
    }

    SDL_SetRenderDrawColor(view->renderer, 0, 0, 0, 255);
    SDL_RenderClear(view->renderer);
    RAD_UpdateIsoMapView(view->iso_map_view, view->renderer);
    if(view->terrain_info_visible)
    {
        RAD_UpdateTerrainInfoView(view->terrain_info_view, view->renderer);
    }
    if(view->unit_info_visible)
    {
        RAD_UpdateUnitInfoView(view->unit_info_view, view->renderer);
    }
    SDL_RenderPresent(view->renderer);
}

void RAD_ViewSetTerrainInfoVisible(RAD_RootView_t *view, bool visible)
{
    view->terrain_info_visible = visible;
}

void RAD_ViewSetUnitInfoVisible(RAD_RootView_t *view, bool visible)
{
    view->unit_info_visible = visible;
}

SDL_Renderer* RAD_ViewRenderer(const RAD_RootView_t *view)
{
    return view->renderer;
}

TTF_Font* RAD_ViewFont(const RAD_RootView_t *view)
{
    return view->font;
}


///
/// Ein SDL-Ereignis aus RAD_UpdateView (view/view.h).
///
static void RAD_ViewHandleEvent(RAD_RootView_t *view, const SDL_Event *event)
{
    // Zuerst die Kamera: rechte Maustaste und Pfeiltasten gehoeren ihr.
    if(RAD_IoCameraControlHandleEvent(&view->camera_control, event))
    {
        return;
    }

    if(event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT)
    {
        int32_t x = 0, y = 0;
        RAD_ToFlatCoordinates(view->map, event->button.x, event->button.y, &x, &y);
        RAD_IoUserInputOnLeftClick(view->user_input, x, y);
    }

    // Alles andere wird vorerst verworfen.
}


///
/// BEISPIEL, nur zum Ansehen der UnitInfoView -- wieder entfernen, sobald echte
/// Einheiten hineinkommen (samt Aufruf in RAD_CreateView).
///
/// Die Zeile ohne Waffen steht zuerst: eine Zeile, die nicht ganz in die Flaeche
/// passt, wird nicht gezeichnet, und alle danach auch nicht (view/unit_info_view.h).
///
static void RAD_ViewAddExampleUnit(RAD_UnitInfoView_t *unit_info_view)
{
    RAD_UnitInfoViewSetName(unit_info_view, "Sturmtrupp");
    // Offizier 7 von 10, Grenadiere 3 x 4 von 8.
    RAD_UnitInfoViewSetHitPoints(unit_info_view, 7 + 3 * 4, 10 + 3 * 8);
    RAD_UnitInfoViewSetMovementRange(unit_info_view, 4);

    RAD_EntityView_t *officer = RAD_UnitInfoViewAddEntity(unit_info_view, 1, 0);
    if(officer != NULL)
    {
        RAD_EntityViewSetName(officer, "Offizier");
        RAD_EntityViewSetHitPoints(officer, 7, 10);
        RAD_EntityViewSetArmor(officer, 2, 5);
        RAD_EntityViewSetStrength(officer, 3, 5);
        RAD_EntityViewSetResistance(officer, 4, 5);
    }

    RAD_EntityView_t *grenadier = RAD_UnitInfoViewAddEntity(unit_info_view, 3, 1);
    if(grenadier != NULL)
    {
        RAD_EntityViewSetName(grenadier, "Grenadier");
        RAD_EntityViewSetHitPoints(grenadier, 4, 8);
        RAD_EntityViewSetArmor(grenadier, 3, 5);
        RAD_EntityViewSetStrength(grenadier, 4, 5);
        RAD_EntityViewSetResistance(grenadier, 2, 5);
        RAD_EntityViewSetWeaponName(grenadier, 0, "Gewehr");
        RAD_EntityViewSetWeaponStrength(grenadier, 0, 3, 5);
        RAD_EntityViewSetWeaponPenetration(grenadier, 0, 2, 5);
        RAD_EntityViewSetWeaponRange(grenadier, 0, 4, 6);
        RAD_EntityViewSetWeaponNumberOfShots(grenadier, 0, 1, 2);
    }
}

///
/// BEISPIEL, nur zum Ansehen der TerrainInfoView -- wieder entfernen, sobald
/// echte Terrains hineinkommen (samt Aufruf in RAD_CreateView).
///
static void RAD_ViewAddExampleTerrain(RAD_TerrainInfoView_t *terrain_info_view)
{
    RAD_TerrainInfoViewSetName(terrain_info_view, "Wald");
    RAD_TerrainInfoViewSetMovementModifier(terrain_info_view, -1);
    RAD_TerrainInfoViewSetCover(terrain_info_view, 2);
}
