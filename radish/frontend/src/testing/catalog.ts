import {
  AdminEntityProfile,
  AdminUnitType,
  AdminWeapon,
  ArmyListEntry,
  EntityProfile,
  Equipment,
  SpeciesCatalog,
  UnitType,
  Weapon,
} from '../app/core/api.service';

/*
 * Testdaten fuer Katalog und Armeen: jede Funktion liefert einen gueltigen
 * Eintrag mit festen Werten, ein Test ueberschreibt nur, worauf es ihm ankommt.
 */

export function profile(overrides: Partial<EntityProfile> = {}): EntityProfile {
  return {
    id: 1,
    name: 'Soldat',
    health: 1,
    armor: 2,
    toughness: 3,
    strength: 3,
    accuracy: 4,
    equipment_slots: 1,
    weapon_slots: 1,
    heavy_weapon_slots: 0,
    super_heavy_weapon_slots: 0,
    cost: 10,
    min_count: 0,
    max_count: 10,
    ...overrides,
  };
}

export function unitType(overrides: Partial<UnitType> = {}): UnitType {
  return {
    id: 100,
    name: 'Trupp',
    min_entities: 1,
    max_entities: 10,
    max_heavy_weapons: null,
    max_super_heavy_weapons: null,
    transport_capacity: 0,
    can_capture_objectives: true,
    movement: 4,
    sight_range: 6,
    sprite: '',
    profiles: [profile()],
    ...overrides,
  };
}

export function weapon(overrides: Partial<Weapon> = {}): Weapon {
  return {
    id: 1000,
    name: 'Gewehr',
    weapon_class: 'standard',
    shots: 2,
    strength: 3,
    min_range: 0,
    max_range: 24,
    armor_penetration: 1,
    cost: 5,
    unit_type_ids: [],
    profile_ids: [],
    ...overrides,
  };
}

export function equipment(overrides: Partial<Equipment> = {}): Equipment {
  return {
    id: 2000,
    name: 'Helm',
    cost: 3,
    health_modifier: 0,
    armor_modifier: 0,
    toughness_modifier: 0,
    strength_modifier: 0,
    accuracy_modifier: 0,
    weapon_shots_modifier: 0,
    weapon_strength_modifier: 0,
    weapon_range_modifier: 0,
    weapon_armor_penetration_modifier: 0,
    unit_type_ids: [],
    profile_ids: [],
    ...overrides,
  };
}

export function species(overrides: Partial<SpeciesCatalog> = {}): SpeciesCatalog {
  return { id: 1, name: 'Menschen', unit_types: [], weapons: [], equipment: [], ...overrides };
}

export function armyEntry(overrides: Partial<ArmyListEntry> = {}): ArmyListEntry {
  return {
    id: 1,
    name: 'Erste Kompanie',
    species_id: 1,
    species_name: 'Menschen',
    unit_count: 3,
    total_cost: 450,
    ...overrides,
  };
}

export function adminWeapon(overrides: Partial<AdminWeapon> = {}): AdminWeapon {
  return { ...weapon(), species_id: 1, species_name: 'Menschen', in_use: false, ...overrides };
}

export function adminProfile(overrides: Partial<AdminEntityProfile> = {}): AdminEntityProfile {
  return {
    ...profile(),
    unit_type_id: 100,
    unit_type_name: 'Trupp',
    species_id: 1,
    species_name: 'Menschen',
    in_use: false,
    ...overrides,
  };
}

export function adminUnitType(overrides: Partial<AdminUnitType> = {}): AdminUnitType {
  return { ...unitType(), species_id: 1, species_name: 'Menschen', in_use: false, profiles: [], ...overrides };
}
