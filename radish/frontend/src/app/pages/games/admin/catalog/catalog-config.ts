import { WEAPON_CLASS_LABELS } from '../../../../core/api.service';
import type { CatalogFilter, CatalogResource, WeaponClass } from '../../../../core/api.service';
import type { CatalogField, CatalogFieldOption } from './catalog-entry-form.component';

const WEAPON_CLASS_OPTIONS: CatalogFieldOption[] = (
  Object.entries(WEAPON_CLASS_LABELS) as [WeaponClass, string][]
).map(([value, label]) => ({ value, label }));

/**
 * Beschreibung der Katalog-Modelle fuer die Admin-Seiten: welche Felder
 * das Formular hat, womit ein neuer Eintrag vorbelegt ist und woran er
 * haengt (parent). Daraus entstehen je Modell die Routen .../new und
 * .../edit (siehe app.routes.ts) und die Seiten CatalogCreateComponent und
 * CatalogEditComponent.
 */
export interface CatalogModel {
  resource: CatalogResource;
  title: string;
  singular: string;
  fields: CatalogField[];
  blank: () => object;
  /** Worunter ein neuer Eintrag angelegt wird -- Spezies bzw. Einheitentyp. */
  parent: 'species' | 'unit-type' | null;
  /** Zusaetzlicher Filter auf der Seite "Ändern", z.B. nach Waffenklasse. */
  extraFilter?: { key: keyof CatalogFilter; label: string; options: CatalogFieldOption[] };
}

/** Was die Suche von jedem Eintrag braucht; die Kontextfelder je nach Modell. */
export interface CatalogEntry {
  id: number;
  name: string;
  in_use?: boolean;
  species_id?: number;
  species_name?: string;
  unit_type_id?: number;
  unit_type_name?: string;
}

// Kampfwerte und Kosten sind bei neuen Eintraegen bewusst leer: die soll
// der Admin eintragen, das Backend hat dafuer keinen Default und lehnt
// leere Werte ab.
export const CATALOG_MODELS: CatalogModel[] = [
  {
    resource: 'species',
    title: 'Spezies',
    singular: 'Spezies',
    parent: null,
    fields: [{ key: 'name', label: 'Name', type: 'text' }],
    blank: () => ({ name: '' }),
  },
  {
    resource: 'unit-types',
    title: 'Einheitentypen',
    singular: 'Einheitentyp',
    parent: 'species',
    fields: [
      { key: 'name', label: 'Name', type: 'text' },
      { key: 'movement', label: 'Bewegung', type: 'number', hint: 'Bewegungsradius in Feldern.' },
      { key: 'min_entities', label: 'Min. Entitäten', type: 'number' },
      { key: 'max_entities', label: 'Max. Entitäten', type: 'number' },
      {
        key: 'max_heavy_weapons',
        label: 'Max. schwere Waffen',
        type: 'number',
        hint: 'Höchstens so viele schwere Waffen je Einheit, über alle Entitäten. Leer = keine Grenze.',
      },
      {
        key: 'max_super_heavy_weapons',
        label: 'Max. super-schwere',
        type: 'number',
        hint: 'Höchstens so viele super-schwere Waffen je Einheit, über alle Entitäten. Leer = keine Grenze.',
      },
      {
        key: 'transport_capacity',
        label: 'Transportkapazität',
        type: 'number',
      },
      { key: 'can_capture_objectives', label: 'Kann Ziele einnehmen', type: 'checkbox' },
    ],
    blank: () => ({
      name: '',
      min_entities: 1,
      max_entities: 10,
      movement: null,
      max_heavy_weapons: null,
      max_super_heavy_weapons: null,
      transport_capacity: 0,
      can_capture_objectives: false,
    }),
  },
  {
    resource: 'profiles',
    title: 'Entitätsprofile',
    singular: 'Entitätsprofil',
    parent: 'unit-type',
    fields: [
      { key: 'name', label: 'Name', type: 'text' },
      { key: 'health', label: 'Lebenspunkte', type: 'number' },
      { key: 'armor', label: 'Rüstung', type: 'number' },
      { key: 'strength', label: 'Stärke', type: 'number' },
      { key: 'accuracy', label: 'Treffsicherheit', type: 'number' },
      { key: 'cost', label: 'Kosten', type: 'number' },
      { key: 'weapon_slots', label: 'Waffenslots', type: 'number' },
      { key: 'heavy_weapon_slots', label: 'Schwere Waffen', type: 'number' },
      { key: 'super_heavy_weapon_slots', label: 'Super-schwere Waffen', type: 'number' },
      {
        key: 'equipment_slots',
        label: 'Ausrüstungsslots',
        type: 'number',
      },
      { key: 'min_count', label: 'Min. je Einheit', type: 'number' },
      { key: 'max_count', label: 'Max. je Einheit', type: 'number' },
    ],
    blank: () => ({
      name: '',
      health: null,
      armor: null,
      strength: null,
      accuracy: null,
      cost: null,
      weapon_slots: 1,
      heavy_weapon_slots: 0,
      super_heavy_weapon_slots: 0,
      equipment_slots: 0,
      min_count: 0,
      max_count: 10,
    }),
  },
  {
    resource: 'weapons',
    title: 'Waffen',
    singular: 'Waffe',
    parent: 'species',
    extraFilter: { key: 'weapon_class', label: 'Klasse', options: WEAPON_CLASS_OPTIONS },
    fields: [
      { key: 'name', label: 'Name', type: 'text' },
      {
        key: 'weapon_class',
        label: 'Klasse',
        type: 'select',
        options: WEAPON_CLASS_OPTIONS,
      },
      { key: 'shots', label: 'Schüsse', type: 'number' },
      { key: 'strength', label: 'Stärke', type: 'number' },
      { key: 'min_range', label: 'Min. Reichweite', type: 'number' },
      { key: 'max_range', label: 'Max. Reichweite', type: 'number' },
      { key: 'armor_penetration', label: 'Durchschlag', type: 'number' },
      { key: 'cost', label: 'Kosten', type: 'number' },
    ],
    blank: () => ({
      name: '',
      shots: null,
      strength: null,
      min_range: 0,
      max_range: null,
      armor_penetration: null,
      cost: null,
      weapon_class: 'standard',
    }),
  },
  {
    resource: 'equipment',
    title: 'Ausrüstung',
    singular: 'Ausrüstung',
    parent: 'species',
    fields: [
      { key: 'name', label: 'Name', type: 'text' },
      { key: 'cost', label: 'Kosten', type: 'number' },
    ],
    blank: () => ({ name: '', cost: null }),
  },
];

export function catalogModel(resource: CatalogResource): CatalogModel {
  const model = CATALOG_MODELS.find((entry) => entry.resource === resource);
  if (!model) {
    throw new Error(`Unbekanntes Katalog-Modell: ${resource}`);
  }
  return model;
}

/** Fuer Fehlermeldungen des Backends, die an einem Feld haengen ("Kosten: ..."). */
export const CATALOG_FIELD_LABELS: Record<string, string> = {
  ...Object.fromEntries(
    CATALOG_MODELS.flatMap((model) => model.fields).map((field) => [field.key, field.label]),
  ),
  species_id: 'Spezies',
  unit_type_id: 'Einheitentyp',
};

/** Kontext in der Trefferliste, z.B. "Trupp · Menschen" bei einem Profil. */
export function entryContext(entry: CatalogEntry): string {
  return [entry.unit_type_name, entry.species_name].filter(Boolean).join(' · ');
}
