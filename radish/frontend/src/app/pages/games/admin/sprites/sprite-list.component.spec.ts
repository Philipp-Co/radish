import { HttpErrorResponse } from '@angular/common/http';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { Router, provideRouter } from '@angular/router';
import { of, throwError } from 'rxjs';

import { adminSprite, pngImage } from '../../../../../testing/admin-media';
import { ApiService } from '../../../../core/api.service';
import { SpriteListComponent } from './sprite-list.component';

describe('SpriteListComponent', () => {
  let api: Record<'listSprites' | 'createSprite', ReturnType<typeof vi.fn>>;
  let fixture: ComponentFixture<SpriteListComponent>;

  async function render(): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      imports: [SpriteListComponent],
      providers: [provideRouter([]), { provide: ApiService, useValue: api }],
    });
    fixture = TestBed.createComponent(SpriteListComponent);
    await fixture.whenStable();
    return fixture.nativeElement as HTMLElement;
  }

  const text = (el: Element) => el.textContent?.replace(/\s+/g, ' ').trim() ?? '';
  const submit = (el: HTMLElement) => el.querySelector<HTMLButtonElement>('button[type=submit]')!;

  async function typeKey(el: HTMLElement, value: string): Promise<void> {
    const input = el.querySelector<HTMLInputElement>('input[name=key]')!;
    input.value = value;
    input.dispatchEvent(new Event('input'));
    await fixture.whenStable();
  }

  beforeEach(() => {
    api = { listSprites: vi.fn(() => of([])), createSprite: vi.fn() };
  });

  it('zeigt vollstaendige Bilder und Entwuerfe mit offenen Punkten und Verwendung', async () => {
    api.listSprites.mockReturnValue(
      of([
        adminSprite({ id: 1, key: 'heavy_tank', complete: true, images: [pngImage()], used_by: ['Panzer (Menschen)'] }),
        adminSprite({ id: 2, key: 'infantry', problems: ['Drehen fehlt.', 'Zielen fehlt.'] }),
      ]),
    );
    const el = await render();

    const items = Array.from(el.querySelectorAll('li')).map(text);
    expect(items).toEqual([
      'heavy_tankvollständig 1 PNG · verwendet von Panzer (Menschen)',
      'infantryEntwurf – 2 offen 0 PNG · nicht verwendet',
    ]);
  });

  it('zeigt einen Hinweis ohne Bilder', async () => {
    expect(text(await render())).toContain('Noch kein Bild hochgeladen.');
  });

  it('zeigt den Ladefehler', async () => {
    api.listSprites.mockReturnValue(
      throwError(() => new HttpErrorResponse({ status: 403, error: { detail: 'Keine Berechtigung.' } })),
    );
    const el = await render();
    expect(el.querySelector('.error')?.textContent).toBe('Keine Berechtigung.');
  });

  it('legt ein Bild mit getrimmtem Schluessel an und oeffnet es', async () => {
    api.createSprite.mockReturnValue(of(adminSprite({ id: 9, key: 'scout' })));
    const el = await render();
    const navigate = vi.spyOn(TestBed.inject(Router), 'navigate').mockResolvedValue(true);
    expect(submit(el).disabled).toBe(true);

    await typeKey(el, '  scout ');
    submit(el).click();
    await fixture.whenStable();

    expect(api.createSprite).toHaveBeenCalledWith('scout');
    expect(navigate).toHaveBeenCalledWith(['/games/admin/sprites', 9]);
  });

  it('nennt bei einem ungueltigen Schluessel das Feld', async () => {
    api.createSprite.mockReturnValue(
      throwError(() => new HttpErrorResponse({ status: 400, error: { key: ['Nur a–z, 0–9 und _.'] } })),
    );
    const el = await render();

    await typeKey(el, 'Gross');
    submit(el).click();
    await fixture.whenStable();

    expect(el.querySelector('.error')?.textContent).toBe('Schlüssel: Nur a–z, 0–9 und _.');
    expect(submit(el).disabled).toBe(false);
  });
});
