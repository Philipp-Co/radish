import { ComponentFixture, TestBed } from '@angular/core/testing';

import { UnitSpriteImage, UnitSpriteService } from '../../../../core/unit-sprite.service';
import { SpriteImagesComponent } from './sprite-images.component';

const TURN: UnitSpriteImage = { name: 'turn.png', url: '/wasm/assets/units/heavy_tank/turn.1.png' };

describe('SpriteImagesComponent', () => {
  let fixture: ComponentFixture<SpriteImagesComponent>;
  let unitSprites: { keys: ReturnType<typeof vi.fn>; images: ReturnType<typeof vi.fn> };
  let saved: string[];

  async function render(spriteKey: string, unitTypeId = 100): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      imports: [SpriteImagesComponent],
      providers: [{ provide: UnitSpriteService, useValue: unitSprites }],
    });
    fixture = TestBed.createComponent(SpriteImagesComponent);
    fixture.componentRef.setInput('spriteKey', spriteKey);
    fixture.componentRef.setInput('unitTypeId', unitTypeId);
    saved = [];
    fixture.componentInstance.save.subscribe((key) => saved.push(key));
    await settle();
    return fixture.nativeElement as HTMLElement;
  }

  /** Promises des Dienstes abwarten und neu zeichnen. */
  async function settle(): Promise<void> {
    await fixture.whenStable();
    await new Promise((resolve) => setTimeout(resolve));
    await fixture.whenStable();
  }

  async function choose(el: HTMLElement, label: string): Promise<void> {
    const select = el.querySelector('select')!;
    select.selectedIndex = Array.from(select.options).findIndex((o) => o.textContent?.trim() === label);
    select.dispatchEvent(new Event('change'));
    await settle();
  }

  const button = (el: HTMLElement, label: string) =>
    Array.from(el.querySelectorAll('button')).find((b) => b.textContent?.trim() === label);

  beforeEach(() => {
    unitSprites = {
      keys: vi.fn().mockResolvedValue(['heavy_tank', 'scout']),
      images: vi.fn((key: string) => Promise.resolve(key === 'heavy_tank' ? [TURN] : [])),
    };
  });

  it('zeigt die Bilder des gespeicherten Schluessels, ohne etwas zu speichern', async () => {
    const el = await render('heavy_tank');
    expect(el.querySelector('img')?.getAttribute('src')).toBe(TURN.url);
    expect(button(el, 'Bild speichern')).toBeUndefined();
  });

  it('speichert erst auf Wunsch und kann die Auswahl verwerfen', async () => {
    const el = await render('heavy_tank');

    await choose(el, 'scout');
    expect(el.textContent).toContain('Noch nicht gespeichert -- gespeichert ist heavy_tank.');
    expect(el.querySelector('.error')?.textContent).toContain('„scout" gibt es nicht in den Assets');

    await choose(el, 'Kein Bild (Rechteck)');
    button(el, 'Bild speichern')!.click();
    expect(saved).toEqual(['']);

    button(el, 'Verwerfen')!.click();
    await settle();
    expect(button(el, 'Bild speichern')).toBeUndefined();
    expect(el.querySelector('img')).not.toBeNull();
  });

  it('verwirft die Auswahl bei einem anderen Einheitentyp mit gleichem Schluessel', async () => {
    const el = await render('heavy_tank');
    await choose(el, 'scout');

    fixture.componentRef.setInput('unitTypeId', 101);
    await settle();

    expect(button(el, 'Bild speichern')).toBeUndefined();
  });

  it('ignoriert eine spaete Antwort fuer einen alten Schluessel', async () => {
    let resolveOld!: (images: UnitSpriteImage[]) => void;
    unitSprites.images.mockImplementationOnce(() => new Promise((resolve) => (resolveOld = resolve)));
    const el = await render('heavy_tank');

    await choose(el, 'scout');
    resolveOld([TURN]);
    await settle();

    expect(el.querySelector('img')).toBeNull();
    expect(el.querySelector('.error')?.textContent).toContain('„scout"');
  });

  it('meldet, wenn das Manifest nicht ladbar ist', async () => {
    unitSprites.images.mockRejectedValue(new Error('offline'));
    const el = await render('heavy_tank');
    expect(el.querySelector('.error')?.textContent).toContain('Bilder nicht ladbar');
  });

  it('oeffnet ein Bild in Originalgroesse', async () => {
    const el = await render('heavy_tank');
    el.querySelector<HTMLButtonElement>('.thumb')!.click();
    await settle();
    expect(el.querySelector('app-image-lightbox')).not.toBeNull();
  });
});
