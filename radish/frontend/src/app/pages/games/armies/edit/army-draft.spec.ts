import { equipment, profile, species, unitType, weapon } from '../../../../../testing/catalog';
import {
  DraftUnit,
  applyProfile,
  composition,
  emptyEntity,
  entityCost,
  heavyWeaponsSummary,
  mayCarry,
  modifiedWeapon,
  modifierSummary,
  newUnit,
  profileStatCells,
  rangeLabel,
  resizeSlots,
  slotsForSaved,
  unitCost,
  unitProblems,
  weaponStatCells,
} from './army-draft';

describe('army-draft', () => {
  describe('Slots', () => {
    it('resizeSlots kuerzt und fuellt mit null auf', () => {
      expect(resizeSlots([1, 2, 3], 2)).toEqual([1, 2]);
      expect(resizeSlots([1], 3)).toEqual([1, null, null]);
    });

    it('slotsForSaved verliert keine gespeicherte Waffe, wenn Slots gestrichen wurden', () => {
      expect(slotsForSaved([1, 2, 3], 1)).toEqual([1, 2, 3]);
      expect(slotsForSaved([1], 2)).toEqual([1, null]);
    });

    it('emptyEntity legt die Slots des Profils leer an', () => {
      const entity = emptyEntity(
        profile({ id: 7, weapon_slots: 2, heavy_weapon_slots: 1, equipment_slots: 3 }),
      );
      expect(entity).toEqual({
        profileId: 7,
        weaponIds: [null, null],
        heavyWeaponIds: [null],
        superHeavyWeaponIds: [],
        equipmentIds: [null, null, null],
      });
    });

    it('applyProfile passt die Slots an und behaelt Belegtes, soweit es passt', () => {
      const entity = emptyEntity(profile({ weapon_slots: 2, equipment_slots: 2 }));
      entity.weaponIds = [1000, 1001];
      entity.equipmentIds = [2000, null];

      applyProfile(entity, profile({ id: 2, weapon_slots: 1, heavy_weapon_slots: 1, equipment_slots: 3 }));

      expect(entity.profileId).toBe(2);
      expect(entity.weaponIds).toEqual([1000]);
      expect(entity.heavyWeaponIds).toEqual([null]);
      expect(entity.equipmentIds).toEqual([2000, null, null]);
    });
  });

  describe('newUnit', () => {
    it('besetzt erst die Mindestzahl je Profil, dann bis min_entities', () => {
      const leader = profile({ id: 1, name: 'Anfuehrer', min_count: 1, max_count: 1 });
      const soldier = profile({ id: 2, name: 'Soldat', min_count: 0, max_count: 9 });
      const type = unitType({ min_entities: 4, profiles: [leader, soldier] });

      const unit = newUnit(type);

      expect(unit.unitTypeId).toBe(type.id);
      expect(unit.entities.map((entity) => entity.profileId)).toEqual([1, 2, 2, 2]);
      expect(unitProblems(species(), type, unit)).toEqual([]);
    });
  });

  describe('Kosten', () => {
    it('rechnet Profil, Waffen aller Klassen und Ausruestung zusammen', () => {
      const p = profile({ cost: 10, weapon_slots: 1, heavy_weapon_slots: 1, equipment_slots: 1 });
      const type = unitType({ profiles: [p] });
      const catalog = species({
        weapons: [weapon({ id: 1, cost: 5 }), weapon({ id: 2, cost: 20, weapon_class: 'heavy' })],
        equipment: [equipment({ id: 3, cost: 3 })],
      });
      const entity = emptyEntity(p);
      entity.weaponIds = [1];
      entity.heavyWeaponIds = [2];
      entity.equipmentIds = [3];

      expect(entityCost(catalog, type, entity)).toBe(38);
      expect(unitCost(catalog, type, { unitTypeId: type.id, entities: [entity, emptyEntity(p)] })).toBe(48);
    });

    it('zaehlt unbekannte ids und leere Slots mit 0', () => {
      const p = profile({ cost: 10 });
      const entity = emptyEntity(p);
      entity.weaponIds = [999];
      expect(entityCost(species(), unitType({ profiles: [p] }), entity)).toBe(10);
    });
  });

  describe('Kurzformen', () => {
    it('composition zaehlt Entitaeten je Profil', () => {
      const leader = profile({ id: 1, name: 'Anfuehrer' });
      const soldier = profile({ id: 2, name: 'Soldat' });
      const type = unitType({ profiles: [leader, soldier, profile({ id: 3, name: 'Sani' })] });
      const unit: DraftUnit = {
        unitTypeId: type.id,
        entities: [emptyEntity(leader), emptyEntity(soldier), emptyEntity(soldier)],
      };
      expect(composition(type, unit)).toBe('1× Anfuehrer · 2× Soldat');
    });

    it('heavyWeaponsSummary listet super-schwere vor schweren Waffen', () => {
      const p = profile({ heavy_weapon_slots: 1, super_heavy_weapon_slots: 1 });
      const catalog = species({
        weapons: [
          weapon({ id: 1, name: 'Kanone', weapon_class: 'heavy' }),
          weapon({ id: 2, name: 'Kampfgeschuetz', weapon_class: 'super_heavy' }),
        ],
      });
      const a = emptyEntity(p);
      a.heavyWeaponIds = [1];
      a.superHeavyWeaponIds = [2];
      const b = emptyEntity(p);
      b.heavyWeaponIds = [1];

      expect(heavyWeaponsSummary(catalog, { unitTypeId: 1, entities: [a, b] })).toBe(
        'Kampfgeschuetz · 2× Kanone',
      );
      expect(heavyWeaponsSummary(catalog, { unitTypeId: 1, entities: [emptyEntity(p)] })).toBe('–');
    });

    it('rangeLabel zeigt die Mindestreichweite nur, wenn es eine gibt', () => {
      expect(rangeLabel(weapon({ min_range: 0, max_range: 24 }))).toBe('24');
      expect(rangeLabel(weapon({ min_range: 6, max_range: 72 }))).toBe('6–72');
    });

    it('modifierSummary zeigt nur Modifikatoren ungleich 0, mit echtem Minuszeichen', () => {
      expect(modifierSummary(equipment({ armor_modifier: 1, weapon_shots_modifier: -1 }))).toBe(
        'RP +1 · Waffen S −1',
      );
      expect(modifierSummary(equipment())).toBe('');
    });
  });

  describe('mayCarry', () => {
    const p = profile({ id: 5 });
    const type = unitType({ id: 50 });

    it('erlaubt alles ohne Einschraenkung', () => {
      expect(mayCarry(weapon(), type, p)).toBe(true);
    });

    it('erlaubt passenden Einheitentyp oder passendes Profil', () => {
      expect(mayCarry(weapon({ unit_type_ids: [50] }), type, p)).toBe(true);
      expect(mayCarry(weapon({ profile_ids: [5] }), type, p)).toBe(true);
    });

    it('verbietet, wenn weder Typ noch Profil passen', () => {
      expect(mayCarry(weapon({ unit_type_ids: [51], profile_ids: [6] }), type, p)).toBe(false);
    });
  });

  describe('unitProblems', () => {
    it('meldet falsche Entitaetenzahl und Profilgrenzen', () => {
      const leader = profile({ id: 1, name: 'Anfuehrer', min_count: 1, max_count: 1 });
      const soldier = profile({ id: 2, name: 'Soldat', max_count: 1 });
      const type = unitType({ min_entities: 1, max_entities: 2, profiles: [leader, soldier] });
      const unit: DraftUnit = {
        unitTypeId: type.id,
        entities: [emptyEntity(soldier), emptyEntity(soldier), emptyEntity(soldier)],
      };

      expect(unitProblems(species(), type, unit)).toEqual([
        '1–2 Entitäten nötig, hat 3',
        'Anfuehrer: mindestens 1',
        'Soldat: höchstens 1',
      ]);
    });

    it('meldet zu viele schwere Waffen je Einheit', () => {
      const p = profile({ heavy_weapon_slots: 1 });
      const type = unitType({ max_heavy_weapons: 1, profiles: [p] });
      const catalog = species({ weapons: [weapon({ id: 1, weapon_class: 'heavy' })] });
      const a = emptyEntity(p);
      const b = emptyEntity(p);
      a.heavyWeaponIds = [1];
      b.heavyWeaponIds = [1];

      expect(unitProblems(catalog, type, { unitTypeId: type.id, entities: [a, b] })).toEqual([
        '2× Schwere Waffe, erlaubt sind 1 je Einheit',
      ]);
    });

    it('meldet ueberzaehlige Slots, falsche Waffenklasse und verbotene Waffen', () => {
      const p = profile({ name: 'Soldat', weapon_slots: 1 });
      const type = unitType({ profiles: [p] });
      const catalog = species({
        weapons: [
          weapon({ id: 1, name: 'Gewehr' }),
          weapon({ id: 2, name: 'Kanone', weapon_class: 'heavy' }),
          weapon({ id: 3, name: 'Flammenwerfer', profile_ids: [99] }),
        ],
      });
      const entity = emptyEntity(p);
      entity.weaponIds = [1, 2, 3];

      expect(unitProblems(catalog, type, { unitTypeId: type.id, entities: [entity] })).toEqual([
        'Entität 1: 3× Waffe, nur 1 Slots',
        'Entität 1: Kanone ist keine Waffe mehr',
        'Entität 1: Soldat darf Flammenwerfer nicht führen',
      ]);
    });

    it('meldet doppelte und verbotene Ausruestung sowie zu viele Gegenstaende', () => {
      const p = profile({ name: 'Soldat', equipment_slots: 2 });
      const type = unitType({ profiles: [p] });
      const catalog = species({
        equipment: [equipment({ id: 1, name: 'Helm' }), equipment({ id: 2, name: 'Jetpack', unit_type_ids: [7] })],
      });
      const entity = emptyEntity(p);
      entity.equipmentIds = [1, 1, 2];

      expect(unitProblems(catalog, type, { unitTypeId: type.id, entities: [entity] })).toEqual([
        'Entität 1: 3 Ausrüstung, nur 2 Slots',
        'Entität 1: Helm doppelt',
        'Entität 1: Soldat darf Jetpack nicht tragen',
      ]);
    });

    it('meldet ein geloeschtes Profil', () => {
      const type = unitType({ min_entities: 0 });
      const entity = emptyEntity(profile({ id: 42 }));
      expect(unitProblems(species(), type, { unitTypeId: type.id, entities: [entity] })).toEqual([
        'Entität 1: Profil gibt es nicht mehr',
      ]);
    });
  });

  describe('Werte mit Ausruestung', () => {
    it('profileStatCells addiert Modifikatoren und faellt nicht unter 0', () => {
      const cells = profileStatCells(profile({ health: 1, armor: 2 }), [
        equipment({ health_modifier: 2 }),
        equipment({ armor_modifier: -5 }),
      ]);
      expect(cells.find((cell) => cell.short === 'LP')).toEqual({
        short: 'LP',
        label: 'Lebenspunkte',
        value: '3',
        delta: 2,
      });
      expect(cells.find((cell) => cell.short === 'RP')).toMatchObject({ value: '0', delta: -2 });
    });

    it('modifiedWeapon haelt die Maximalreichweite ueber der Mindestreichweite', () => {
      const modified = modifiedWeapon(weapon({ min_range: 6, max_range: 12, shots: 1 }), [
        equipment({ weapon_range_modifier: -20, weapon_shots_modifier: -3 }),
      ]);
      expect(modified.max_range).toBe(6);
      expect(modified.shots).toBe(0);
    });

    it('weaponStatCells zeigt Werte und Aenderung je Wert', () => {
      const cells = weaponStatCells(weapon({ max_range: 24 }), [equipment({ weapon_range_modifier: 6 })]);
      expect(cells.find((cell) => cell.short === 'RW')).toMatchObject({ value: '30', delta: 6 });
      expect(cells.find((cell) => cell.short === 'S')).toMatchObject({ delta: 0 });
    });
  });
});
