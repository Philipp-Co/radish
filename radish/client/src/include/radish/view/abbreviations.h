#ifndef __RAD_VIEW_ABBREVIATIONS_H__
#define __RAD_VIEW_ABBREVIATIONS_H__

///
/// Die Abkuerzungen, mit denen die Views Attribute beschriften -- grundsaetzlich
/// genau zwei Buchstaben, damit die Beschriftungen gleich breit sind und neben
/// einem Balken (view/labeled_bar_view.h) wenig Platz brauchen.
///
/// Eine neue Abkuerzung gehoert hierher und nicht als Text in eine View.
///

/// Einheit (view/unit_info_view.h)
#define RAD_VIEW_ABBREVIATION_MOVEMENT_RANGE    "MV"    // Bewegungsreichweite

/// Terrain (view/terrain_info_view.h)
#define RAD_VIEW_ABBREVIATION_COVER             "CV"    // Deckung

/// Einheit und Terrain (view/unit_info_view.h, view/terrain_info_view.h)
/// RAD_VIEW_ABBREVIATION_MOVEMENT_RANGE -- beim Terrain ihr Modifier

/// Einheit und Entitaet (view/unit_info_view.h, view/entity_view.h)
#define RAD_VIEW_ABBREVIATION_HIT_POINTS        "LP"    // Lebenspunkte

/// Entitaet (view/entity_view.h)
#define RAD_VIEW_ABBREVIATION_ARMOR             "AR"    // Ruestung
#define RAD_VIEW_ABBREVIATION_RESISTANCE        "RS"    // Widerstand

/// Entitaet und Waffe (view/entity_view.h, view/weapon_view.h)
#define RAD_VIEW_ABBREVIATION_STRENGTH          "ST"    // Staerke

/// Waffe (view/weapon_view.h)
#define RAD_VIEW_ABBREVIATION_PENETRATION       "AP"    // Durchschlagskraft
#define RAD_VIEW_ABBREVIATION_RANGE             "RG"    // Reichweite
#define RAD_VIEW_ABBREVIATION_NUMBER_OF_SHOTS   "SH"    // Anzahl Schuesse

#endif
