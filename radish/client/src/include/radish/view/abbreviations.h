#ifndef __RAD_VIEW_ABBREVIATIONS_H__
#define __RAD_VIEW_ABBREVIATIONS_H__

///
/// Die Abkuerzungen, mit denen die Views Attribute beschriften -- grundsaetzlich
/// genau zwei Buchstaben, damit die Beschriftungen gleich breit sind und wenig
/// Platz brauchen.
///
/// Eine neue Abkuerzung gehoert hierher und nicht als Text in eine View.
///

/// Einheit (view/unit_info_view.h)
#define RAD_VIEW_ABBREVIATION_HIT_POINTS        "LP"    // Lebenspunkte

/// Terrain (view/terrain_info_view.h)
#define RAD_VIEW_ABBREVIATION_COVER             "CV"    // Deckung

/// Einheit und Terrain (view/unit_info_view.h, view/terrain_info_view.h) --
/// beim Terrain ihr Modifier
#define RAD_VIEW_ABBREVIATION_MOVEMENT_RANGE    "MV"    // Bewegungsreichweite

#endif
