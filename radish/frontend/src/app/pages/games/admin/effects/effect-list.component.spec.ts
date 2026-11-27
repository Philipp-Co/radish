import { HttpErrorResponse } from '@angular/common/http';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { Router, provideRouter } from '@angular/router';
import { of, throwError } from 'rxjs';

import { adminEffect, pngImage } from '../../../../../testing/admin-media';
import { ApiService } from '../../../../core/api.service';
import { EffectListComponent } from './effect-list.component';

describe('EffectListComponent', () => {
  let api: Record<'listEffects' | 'createEffect', ReturnType<typeof vi.fn>>;
  let fixture: ComponentFixture<EffectListComponent>;

  async function render(): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      imports: [EffectListComponent],
      providers: [provideRouter([]), { provide: ApiService, useValue: api }],
    });
    fixture = TestBed.createComponent(EffectListComponent);
    await fixture.whenStable();
    return fixture.nativeElement as HTMLElement;
  }

  const text = (el: Element) => el.textContent?.replace(/\s+/g, ' ').trim() ?? '';
  const submit = (el: HTMLElement) => el.querySelector<HTMLButtonElement>('button[type=submit]')!;

  async function setInput(el: HTMLElement, name: string, value: string): Promise<void> {
    const input = el.querySelector<HTMLInputElement>(`input[name=${name}]`)!;
    input.value = value;
    input.dispatchEvent(new Event('input'));
    await fixture.whenStable();
  }

  beforeEach(() => {
    api = { listEffects: vi.fn(() => of([])), createEffect: vi.fn() };
  });

  it('zeigt Art, PNG-Zahl und Dauer je Effekt', async () => {
    api.listEffects.mockReturnValue(
      of([
        adminEffect({ id: 1, key: 'boom', complete: true, images: [pngImage()], duration_ms: 1200 }),
        adminEffect({ id: 2, key: 'puff', problems: ['sw fehlt.'] }),
      ]),
    );
    const el = await render();

    expect(Array.from(el.querySelectorAll('li')).map(text)).toEqual([
      'boomvollständig Explosion · 1 PNG · 1200 ms',
      'puffEntwurf – 1 offen Explosion · 0 PNG · 1000 ms',
    ]);
  });

  it('zeigt den Ladefehler', async () => {
    api.listEffects.mockReturnValue(throwError(() => new HttpErrorResponse({ status: 500 })));
    const el = await render();
    expect(el.querySelector('.error')?.textContent).toBe('Effekte konnten nicht geladen werden.');
  });

  it('legt einen Effekt mit den Werten aus dem Formular an', async () => {
    api.createEffect.mockReturnValue(of(adminEffect({ id: 8 })));
    const el = await render();
    const navigate = vi.spyOn(TestBed.inject(Router), 'navigate').mockResolvedValue(true);

    await setInput(el, 'key', ' boom ');
    await setInput(el, 'draw_width', '300');
    await setInput(el, 'duration_ms', '1500');
    submit(el).click();
    await fixture.whenStable();

    expect(api.createEffect).toHaveBeenCalledWith('boom', { kind: 'explosion', draw_width: 300, duration_ms: 1500 });
    expect(navigate).toHaveBeenCalledWith(['/games/admin/effects', 8]);
  });

  it('nennt bei Fehlern das Feld mit seiner Bezeichnung', async () => {
    api.createEffect.mockReturnValue(
      throwError(() => new HttpErrorResponse({ status: 400, error: { duration_ms: ['Höchstens 10000.'] } })),
    );
    const el = await render();
    await setInput(el, 'key', 'boom');
    submit(el).click();
    await fixture.whenStable();

    expect(el.querySelector('.error')?.textContent).toBe('Dauer: Höchstens 10000.');
  });
});
