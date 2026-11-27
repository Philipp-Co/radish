import { ComponentFixture, TestBed } from '@angular/core/testing';

import { UnitSpriteService } from '../../../../core/unit-sprite.service';
import { CatalogDraft, CatalogEntryFormComponent, CatalogField } from './catalog-entry-form.component';

const FIELDS: CatalogField[] = [
  { key: 'name', label: 'Name', type: 'text' },
  { key: 'cost', label: 'Kosten', type: 'number' },
  { key: 'can_capture_objectives', label: 'Kann Ziele einnehmen', type: 'checkbox' },
];
const SPRITE_FIELD: CatalogField = { key: 'sprite', label: 'Bild', type: 'select', optionsFrom: 'unit-sprites' };

describe('CatalogEntryFormComponent', () => {
  let fixture: ComponentFixture<CatalogEntryFormComponent>;
  let keys: ReturnType<typeof vi.fn>;
  let saved: CatalogDraft[];

  async function render(inputs: Record<string, unknown>): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      imports: [CatalogEntryFormComponent],
      providers: [{ provide: UnitSpriteService, useValue: { keys } }],
    });
    fixture = TestBed.createComponent(CatalogEntryFormComponent);
    for (const [name, value] of Object.entries(inputs)) {
      fixture.componentRef.setInput(name, value);
    }
    saved = [];
    fixture.componentInstance.save.subscribe((draft) => saved.push(draft));
    await fixture.whenStable();
    return fixture.nativeElement as HTMLElement;
  }

  async function setInput(name: string, value: unknown): Promise<void> {
    fixture.componentRef.setInput(name, value);
    await fixture.whenStable();
  }

  const button = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('button')).find((b) => b.textContent?.trim() === label);

  beforeEach(() => {
    keys = vi.fn().mockResolvedValue(['heavy_tank', 'scout']);
  });

  it('uebernimmt nur die Felder des Formulars und sendet sie unveraendert', async () => {
    const el = await render({ fields: FIELDS, value: { id: 3, name: 'Trupp', cost: 5, in_use: true } });

    expect(el.querySelector<HTMLInputElement>('input[type=text]')!.value).toBe('Trupp');
    button(el, 'Speichern')!.click();

    // Fehlende Felder sind null, fremde (id, in_use) fehlen.
    expect(saved).toEqual([{ name: 'Trupp', cost: 5, can_capture_objectives: null }]);
  });

  it('schickt die Eingaben des Nutzers', async () => {
    const el = await render({ fields: FIELDS, value: { name: '', cost: null, can_capture_objectives: false } });

    const name = el.querySelector<HTMLInputElement>('input[type=text]')!;
    name.value = 'Spaeher';
    name.dispatchEvent(new Event('input'));
    const cost = el.querySelector<HTMLInputElement>('input[type=number]')!;
    cost.value = '12';
    cost.dispatchEvent(new Event('input'));
    el.querySelector<HTMLInputElement>('input[type=checkbox]')!.click();
    await fixture.whenStable();
    button(el, 'Speichern')!.click();

    expect(saved).toEqual([{ name: 'Spaeher', cost: 12, can_capture_objectives: true }]);
  });

  it('behaelt die Eingaben, wenn nur busy oder error wechseln', async () => {
    const el = await render({ fields: FIELDS, value: { name: 'Alt' } });
    const name = el.querySelector<HTMLInputElement>('input[type=text]')!;
    name.value = 'Neu';
    name.dispatchEvent(new Event('input'));

    await setInput('busy', true);
    await setInput('error', 'Kosten: Pflichtfeld.');
    await setInput('busy', false);

    expect(el.querySelector('.error')?.textContent).toBe('Kosten: Pflichtfeld.');
    expect(fixture.componentInstance.draft['name']).toBe('Neu');
  });

  it('setzt die Eingaben bei einem neuen Wert zurueck', async () => {
    await render({ fields: FIELDS, value: { name: 'Alt' } });
    fixture.componentInstance.draft['name'] = 'Geaendert';

    await setInput('value', { name: 'Frisch geladen' });

    expect(fixture.componentInstance.draft['name']).toBe('Frisch geladen');
  });

  it('zeigt beim Anlegen kein Loeschen, bei verwendeten Eintraegen ein gesperrtes', async () => {
    let el = await render({ fields: FIELDS, value: {}, isNew: true });
    expect(button(el, 'Anlegen')).toBeDefined();
    expect(button(el, 'Löschen')).toBeUndefined();

    TestBed.resetTestingModule();
    el = await render({ fields: FIELDS, value: {}, inUse: true });
    expect(button(el, 'Löschen')!.disabled).toBe(true);
    expect(el.textContent).toContain('In Armeen verwendet: nicht löschbar.');
  });

  it('bietet die Einheitenbilder an, ein fehlendes Bild leer statt null', async () => {
    const el = await render({ fields: [SPRITE_FIELD], value: { sprite: null } });
    await vi.waitFor(() => expect(el.querySelectorAll('option').length).toBe(3));

    expect(Array.from(el.querySelectorAll('option')).map((o) => o.textContent?.trim())).toEqual([
      'Kein Bild (Rechteck)',
      'heavy_tank',
      'scout',
    ]);
    button(el, 'Speichern')!.click();
    expect(saved).toEqual([{ sprite: '' }]);
  });

  it('behaelt ein gespeichertes Bild, das in den Assets fehlt, und meldet Ladefehler', async () => {
    keys.mockRejectedValue(new Error('offline'));
    const el = await render({ fields: [SPRITE_FIELD], value: { sprite: 'alt' } });
    await vi.waitFor(() => expect(el.querySelector('label .error')).not.toBeNull());

    expect(el.querySelector('label .error')?.textContent).toContain('Bilder nicht ladbar');
    expect(Array.from(el.querySelectorAll('option')).map((o) => o.textContent?.trim())).toEqual([
      'Kein Bild (Rechteck)',
      'alt (nicht in den Assets)',
    ]);
  });
});
