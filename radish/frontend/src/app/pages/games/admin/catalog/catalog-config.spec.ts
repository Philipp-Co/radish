import { CATALOG_FIELD_LABELS, CATALOG_MODELS, catalogModel, entryContext } from './catalog-config';

describe('catalog-config', () => {
  it.each(CATALOG_MODELS.map((model) => [model.resource, model] as const))(
    '%s: ein neuer Eintrag belegt genau die Felder des Formulars vor',
    (_resource, model) => {
      expect(Object.keys(model.blank()).sort()).toEqual(model.fields.map((field) => field.key).sort());
    },
  );

  it('liefert bei jedem Aufruf ein frisches Objekt (setzt das Formular zurueck)', () => {
    const model = catalogModel('weapons');
    expect(model.blank()).not.toBe(model.blank());
  });

  it('kennt nur die fuenf Katalog-Modelle', () => {
    expect(catalogModel('profiles').parent).toBe('unit-type');
    expect(() => catalogModel('gibt-es-nicht' as never)).toThrow('Unbekanntes Katalog-Modell');
  });

  it('benennt Waffen-Modifikatoren in Fehlermeldungen eindeutig', () => {
    // Im Formular heissen beide "Stärke" -- in einer Meldung fehlt der Abschnitt.
    expect(CATALOG_FIELD_LABELS['strength_modifier']).toBe('Stärke');
    expect(CATALOG_FIELD_LABELS['weapon_strength_modifier']).toBe('Waffen: Stärke');
    expect(CATALOG_FIELD_LABELS['unit_type_id']).toBe('Einheitentyp');
  });

  it('entryContext zeigt Einheitentyp und Spezies, soweit vorhanden', () => {
    expect(entryContext({ id: 1, name: 'Soldat', unit_type_name: 'Trupp', species_name: 'Menschen' })).toBe(
      'Trupp · Menschen',
    );
    expect(entryContext({ id: 1, name: 'Gewehr', species_name: 'Menschen' })).toBe('Menschen');
    expect(entryContext({ id: 1, name: 'Menschen' })).toBe('');
  });
});
