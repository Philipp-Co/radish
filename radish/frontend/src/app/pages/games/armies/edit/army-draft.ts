import {
  EntityProfile,
  Equipment,
  EquipmentModifiers,
  ProfileRestricted,
  SpeciesCatalog,
  UnitType,
  Weapon,
  WeaponClass,
} from '../../../../core/api.service';

/**
 * Arbeitsstand einer Armee im Editor (siehe ArmyEditComponent) und die
 * Rechnungen darauf. Rein, ohne Angular -- Uebersicht und Detailansicht
 * einer Einheit nutzen dieselben Funktionen.
 */

/** Eine Entitaet: Waffen je Klasse und Ausruestung in Slots, null = leer. */
export interface DraftEntity {
  profileId: number;
  weaponIds: (number | null)[];
  heavyWeaponIds: (number | null)[];
  superHeavyWeaponIds: (number | null)[];
  equipmentIds: (number | null)[];
}

export interface DraftUnit {
  unitTypeId: number;
  entities: DraftEntity[];
}

/**
 * Die Waffenklassen einer Entitaet in Anzeige-Reihenfolge: welche Slots des
 * Profils sie belegen und wo ihre Belegung im DraftEntity steht.
 */
export const ENTITY_WEAPON_CLASSES: {
  weaponClass: WeaponClass;
  label: string;
  slots: (profile: EntityProfile) => number;
  /** Hoechstzahl je Einheit (null = keine Grenze), nur fuer schwere Klassen. */
  unitLimit: (type: UnitType) => number | null;
  ids: (entity: DraftEntity) => (number | null)[];
  setIds: (entity: DraftEntity, ids: (number | null)[]) => void;
}[] = [
  {
    weaponClass: 'standard',
    label: 'Waffe',
    slots: (profile) => profile.weapon_slots,
    unitLimit: () => null,
    ids: (entity) => entity.weaponIds,
    setIds: (entity, ids) => (entity.weaponIds = ids),
  },
  {
    weaponClass: 'heavy',
    label: 'Schwere Waffe',
    slots: (profile) => profile.heavy_weapon_slots,
    unitLimit: (type) => type.max_heavy_weapons,
    ids: (entity) => entity.heavyWeaponIds,
    setIds: (entity, ids) => (entity.heavyWeaponIds = ids),
  },
  {
    weaponClass: 'super_heavy',
    label: 'Super-schwere Waffe',
    slots: (profile) => profile.super_heavy_weapon_slots,
    unitLimit: (type) => type.max_super_heavy_weapons,
    ids: (entity) => entity.superHeavyWeaponIds,
    setIds: (entity, ids) => (entity.superHeavyWeaponIds = ids),
  },
];

export function weaponsOfClass(species: SpeciesCatalog, weaponClass: WeaponClass): Weapon[] {
  return species.weapons.filter((weapon) => weapon.weapon_class === weaponClass);
}

export function profileOf(type: UnitType, id: number): EntityProfile | undefined {
  return type.profiles.find((profile) => profile.id === id);
}

export function resizeSlots(ids: (number | null)[], slots: number): (number | null)[] {
  return Array.from({ length: slots }, (_, i) => ids[i] ?? null);
}

/**
 * Slots fuer eine gespeicherte Belegung: mindestens so viele, wie der
 * Katalog vorsieht, aber nie weniger als belegt sind. Hat der Admin Slots
 * gestrichen, verschwinden die ueberzaehligen Waffen so nicht still --
 * unitProblems meldet sie, und der Spieler entscheidet, was wegfaellt.
 */
export function slotsForSaved(ids: number[], slots: number): (number | null)[] {
  return resizeSlots(ids, Math.max(slots, ids.length));
}

export function emptyEntity(profile: EntityProfile): DraftEntity {
  return {
    profileId: profile.id,
    weaponIds: resizeSlots([], profile.weapon_slots),
    heavyWeaponIds: resizeSlots([], profile.heavy_weapon_slots),
    superHeavyWeaponIds: resizeSlots([], profile.super_heavy_weapon_slots),
    equipmentIds: resizeSlots([], profile.equipment_slots),
  };
}

/** Profilwechsel: alle Slots an das neue Profil anpassen, Belegtes bleibt soweit moeglich. */
export function applyProfile(entity: DraftEntity, profile: EntityProfile): void {
  entity.profileId = profile.id;
  for (const weaponClass of ENTITY_WEAPON_CLASSES) {
    weaponClass.setIds(entity, resizeSlots(weaponClass.ids(entity), weaponClass.slots(profile)));
  }
  entity.equipmentIds = resizeSlots(entity.equipmentIds, profile.equipment_slots);
}

/** Das erste Profil, von dem die Einheit noch eine Entitaet aufnehmen darf. */
export function profileWithRoom(type: UnitType, unit: DraftUnit): EntityProfile {
  return (
    type.profiles.find((profile) => countOf(unit, profile) < profile.max_count) ?? type.profiles[0]
  );
}

/**
 * Neue Einheit mit gueltiger Grundbesetzung: erst jedes Profil mit seiner
 * Mindestzahl, dann bis min_entities mit dem ersten Profil aufgefuellt, das
 * noch Platz hat.
 */
export function newUnit(type: UnitType): DraftUnit {
  const unit: DraftUnit = { unitTypeId: type.id, entities: [] };
  for (const profile of type.profiles) {
    for (let i = 0; i < profile.min_count; i++) {
      unit.entities.push(emptyEntity(profile));
    }
  }
  while (unit.entities.length < type.min_entities) {
    unit.entities.push(emptyEntity(profileWithRoom(type, unit)));
  }
  return unit;
}

function countOf(unit: DraftUnit, profile: EntityProfile): number {
  return unit.entities.filter((entity) => entity.profileId === profile.id).length;
}

function priceOf(items: { id: number; cost: number }[], id: number | null): number {
  return items.find((item) => item.id === id)?.cost ?? 0;
}

/** Wie viele Waffen einer Klasse die Einheit ueber alle Entitaeten fuehrt. */
export function unitWeaponCount(
  unit: DraftUnit,
  weaponClass: (typeof ENTITY_WEAPON_CLASSES)[number],
): number {
  return unit.entities.reduce(
    (sum, entity) => sum + weaponClass.ids(entity).filter((id) => id !== null).length,
    0,
  );
}

function allWeaponIds(entity: DraftEntity): (number | null)[] {
  return ENTITY_WEAPON_CLASSES.flatMap((weaponClass) => weaponClass.ids(entity));
}

/**
 * Kosten live aus dem Katalog -- dieselbe Rechnung wie im Backend
 * (models.Army.total_cost), das die gespeicherte Armee mit total_cost
 * zurueckliefert.
 */
export function entityCost(species: SpeciesCatalog, type: UnitType, entity: DraftEntity): number {
  return (
    (profileOf(type, entity.profileId)?.cost ?? 0) +
    allWeaponIds(entity).reduce<number>((sum, id) => sum + priceOf(species.weapons, id), 0) +
    entity.equipmentIds.reduce<number>((sum, id) => sum + priceOf(species.equipment, id), 0)
  );
}

export function unitCost(species: SpeciesCatalog, type: UnitType, unit: DraftUnit): number {
  return unit.entities.reduce((sum, entity) => sum + entityCost(species, type, entity), 0);
}

/**
 * Schwere und super-schwere Waffen aller Entitaeten in Kurzform, z.B.
 * "Kampfgeschütz · 2× Kanone"; "–" ohne.
 */
export function heavyWeaponsSummary(species: SpeciesCatalog, unit: DraftUnit): string {
  const counts = new Map<string, number>();
  for (const entity of unit.entities) {
    for (const id of [...entity.superHeavyWeaponIds, ...entity.heavyWeaponIds]) {
      const name = species.weapons.find((weapon) => weapon.id === id)?.name;
      if (name) {
        counts.set(name, (counts.get(name) ?? 0) + 1);
      }
    }
  }
  const parts = [...counts].map(([name, n]) => (n > 1 ? `${n}× ${name}` : name));
  return parts.length > 0 ? parts.join(' · ') : '–';
}

/** Zusammensetzung in Kurzform, z.B. "1× Anführer · 5× Soldat". */
export function composition(type: UnitType, unit: DraftUnit): string {
  return type.profiles
    .map((profile) => ({ profile, count: countOf(unit, profile) }))
    .filter(({ count }) => count > 0)
    .map(({ profile, count }) => `${count}× ${profile.name}`)
    .join(' · ');
}

/**
 * Was an der Einheit gegen die Regeln des Katalogs verstoesst -- dieselben
 * Regeln, die das Backend beim Speichern prueft (serializers.
 * ArmyWriteSerializer), hier nur zur Anzeige vorab.
 */
export function unitProblems(species: SpeciesCatalog, type: UnitType, unit: DraftUnit): string[] {
  const problems: string[] = [];
  const count = unit.entities.length;
  if (count < type.min_entities || count > type.max_entities) {
    problems.push(`${type.min_entities}–${type.max_entities} Entitäten nötig, hat ${count}`);
  }
  for (const profile of type.profiles) {
    const n = countOf(unit, profile);
    if (n < profile.min_count) {
      problems.push(`${profile.name}: mindestens ${profile.min_count}`);
    } else if (n > profile.max_count) {
      problems.push(`${profile.name}: höchstens ${profile.max_count}`);
    }
  }

  for (const weaponClass of ENTITY_WEAPON_CLASSES) {
    const limit = weaponClass.unitLimit(type);
    const used = unitWeaponCount(unit, weaponClass);
    if (limit !== null && used > limit) {
      problems.push(`${used}× ${weaponClass.label}, erlaubt sind ${limit} je Einheit`);
    }
  }

  const weapon = (id: number) => species.weapons.find((entry) => entry.id === id);
  const filled = (ids: (number | null)[]) => ids.filter((id): id is number => id !== null);
  unit.entities.forEach((entity, index) => {
    const label = `Entität ${index + 1}`;
    const profile = profileOf(type, entity.profileId);
    if (!profile) {
      problems.push(`${label}: Profil gibt es nicht mehr`);
      return;
    }
    for (const weaponClass of ENTITY_WEAPON_CLASSES) {
      const ids = filled(weaponClass.ids(entity));
      const slots = weaponClass.slots(profile);
      if (ids.length > slots) {
        problems.push(`${label}: ${ids.length}× ${weaponClass.label}, nur ${slots} Slots`);
      }
      for (const id of ids) {
        const found = weapon(id);
        if (found && found.weapon_class !== weaponClass.weaponClass) {
          problems.push(`${label}: ${found.name} ist keine ${weaponClass.label} mehr`);
        } else if (found && !mayCarry(found, type, profile)) {
          problems.push(`${label}: ${profile.name} darf ${found.name} nicht führen`);
        }
      }
    }
    problems.push(...equipmentProblems(species, type, profile, filled(entity.equipmentIds), label));
  });
  return problems;
}

/** Slots, jeder Gegenstand nur einmal, und ob das Profil ihn tragen darf. */
function equipmentProblems(
  species: SpeciesCatalog,
  type: UnitType,
  profile: EntityProfile,
  ids: number[],
  label: string,
): string[] {
  const problems: string[] = [];
  if (ids.length > profile.equipment_slots) {
    problems.push(`${label}: ${ids.length} Ausrüstung, nur ${profile.equipment_slots} Slots`);
  }
  const seen = new Set<number>();
  for (const id of ids) {
    const item = species.equipment.find((entry) => entry.id === id);
    if (seen.has(id)) {
      problems.push(`${label}: ${item?.name ?? 'Ausrüstung'} doppelt`);
    } else if (item && !mayCarry(item, type, profile)) {
      problems.push(`${label}: ${profile.name} darf ${item.name} nicht tragen`);
    }
    seen.add(id);
  }
  return problems;
}

/**
 * Ob eine Entitaet mit diesem Profil (vom Einheitentyp type) die Waffe bzw.
 * den Gegenstand tragen darf -- dieselbe Regel wie im Backend
 * (models.ProfileRestricted.allows).
 */
export function mayCarry(entry: ProfileRestricted, type: UnitType, profile: EntityProfile): boolean {
  if (entry.unit_type_ids.length === 0 && entry.profile_ids.length === 0) {
    return true;
  }
  return entry.unit_type_ids.includes(type.id) || entry.profile_ids.includes(profile.id);
}

export function weaponStats(weapon: Weapon): string {
  return (
    `S ${weapon.shots} · ST ${weapon.strength} · RW ${rangeLabel(weapon)} · ` +
    `DK ${weapon.armor_penetration}`
  );
}

/** Reichweite als Spanne, z.B. "6–72"; ohne Mindestreichweite nur "24". */
export function rangeLabel(weapon: Weapon): string {
  return weapon.min_range > 0 ? `${weapon.min_range}–${weapon.max_range}` : `${weapon.max_range}`;
}

/** Die Gegenstaende einer Entitaet, ohne leere Slots und unbekannte ids. */
export function equipmentOf(species: SpeciesCatalog, entity: DraftEntity): Equipment[] {
  return entity.equipmentIds
    .map((id) => species.equipment.find((item) => item.id === id))
    .filter((item): item is Equipment => item !== undefined);
}

function modifierSum(items: Equipment[], modifier: keyof EquipmentModifiers): number {
  return items.reduce((sum, item) => sum + item[modifier], 0);
}

/** Ein Wert fuer die Anzeige: Kuerzel, Endwert und die Aenderung durch Ausruestung. */
export interface StatCell {
  short: string;
  label: string;
  value: string;
  delta: number;
}

type ProfileStat = 'health' | 'armor' | 'toughness' | 'strength' | 'accuracy';

const PROFILE_STATS: { key: ProfileStat; short: string; label: string; modifier: keyof EquipmentModifiers }[] = [
  { key: 'health', short: 'LP', label: 'Lebenspunkte', modifier: 'health_modifier' },
  { key: 'armor', short: 'RP', label: 'Rüstungspunkte', modifier: 'armor_modifier' },
  { key: 'toughness', short: 'WI', label: 'Widerstand', modifier: 'toughness_modifier' },
  { key: 'strength', short: 'ST', label: 'Stärke', modifier: 'strength_modifier' },
  { key: 'accuracy', short: 'TS', label: 'Treffsicherheit', modifier: 'accuracy_modifier' },
];

/**
 * Die Werte des Profils mit allen Modifikatoren der Ausruestung; kein Wert
 * faellt unter 0. Dieselben Regeln wie beim Spielstart im Backend
 * (radish/backend/api/domain/equipment_modifiers.py) -- gespielt wird mit
 * dessen Werten.
 */
export function profileStatCells(profile: EntityProfile, items: Equipment[]): StatCell[] {
  return PROFILE_STATS.map(({ key, short, label, modifier }) => {
    const value = Math.max(0, profile[key] + modifierSum(items, modifier));
    return { short, label, value: `${value}`, delta: value - profile[key] };
  });
}

/**
 * Eine Waffe mit den weapon_*-Modifikatoren der Ausruestung ihres Traegers.
 * Kein Wert faellt unter 0, die Maximalreichweite nicht unter die
 * Mindestreichweite.
 */
export function modifiedWeapon(weapon: Weapon, items: Equipment[]): Weapon {
  return {
    ...weapon,
    shots: Math.max(0, weapon.shots + modifierSum(items, 'weapon_shots_modifier')),
    strength: Math.max(0, weapon.strength + modifierSum(items, 'weapon_strength_modifier')),
    max_range: Math.max(weapon.min_range, weapon.max_range + modifierSum(items, 'weapon_range_modifier')),
    armor_penetration: Math.max(
      0,
      weapon.armor_penetration + modifierSum(items, 'weapon_armor_penetration_modifier'),
    ),
  };
}

/** Die Werte einer Waffe mit der Ausruestung ihres Traegers, wie weaponStats. */
export function weaponStatCells(weapon: Weapon, items: Equipment[]): StatCell[] {
  const modified = modifiedWeapon(weapon, items);
  return [
    { short: 'S', label: 'Schüsse', value: `${modified.shots}`, delta: modified.shots - weapon.shots },
    { short: 'ST', label: 'Stärke', value: `${modified.strength}`, delta: modified.strength - weapon.strength },
    {
      short: 'RW',
      label: 'Reichweite',
      value: rangeLabel(modified),
      delta: modified.max_range - weapon.max_range,
    },
    {
      short: 'DK',
      label: 'Durchschlagskraft',
      value: `${modified.armor_penetration}`,
      delta: modified.armor_penetration - weapon.armor_penetration,
    },
  ];
}

/** Die Modifikatoren eines Gegenstands in Kurzform, z.B. "RP +1 · S −1"; "" ohne. */
export function modifierSummary(item: Equipment): string {
  const parts: [string, number][] = [
    ...PROFILE_STATS.map(({ short, modifier }): [string, number] => [short, item[modifier]]),
    ['Waffen S', item.weapon_shots_modifier],
    ['Waffen ST', item.weapon_strength_modifier],
    ['Waffen RW', item.weapon_range_modifier],
    ['Waffen DK', item.weapon_armor_penetration_modifier],
  ];
  return parts
    .filter(([, value]) => value !== 0)
    .map(([short, value]) => `${short} ${value > 0 ? '+' : '−'}${Math.abs(value)}`)
    .join(' · ');
}
