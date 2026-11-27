import { HttpErrorResponse } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';
import { Component } from '@angular/core';
import { Router, provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { of, throwError } from 'rxjs';

import { armyEntry } from '../../../../testing/catalog';
import { ApiService } from '../../../core/api.service';
import { GameJoinComponent } from './game-join.component';

@Component({ template: '' })
class StubPage {}

describe('GameJoinComponent', () => {
  let api: Record<'findGame' | 'joinGame' | 'listArmies', ReturnType<typeof vi.fn>>;
  let harness: RouterTestingHarness;

  async function open(url: string): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      providers: [
        provideRouter([
          { path: 'games/join', component: GameJoinComponent },
          { path: 'games/list', component: StubPage },
          { path: 'games/lobby', component: StubPage },
        ]),
        { provide: ApiService, useValue: api },
      ],
    });
    harness = await RouterTestingHarness.create();
    await harness.navigateByUrl(url);
    return harness.routeNativeElement!;
  }

  const stable = () => harness.fixture.whenStable();
  const text = (el: HTMLElement) => el.textContent?.replace(/\s+/g, ' ') ?? '';
  const submit = (el: HTMLElement) => el.querySelector<HTMLButtonElement>('button[type=submit]')!;
  const radio = (el: HTMLElement, armyName: string) =>
    Array.from(el.querySelectorAll('li')).find((li) => li.textContent?.includes(armyName))!
      .querySelector<HTMLInputElement>('input[type=radio]')!;

  async function fillIn(el: HTMLElement, password: string, armyName: string): Promise<void> {
    const input = el.querySelector<HTMLInputElement>('input[type=password]')!;
    input.value = password;
    input.dispatchEvent(new Event('input'));
    radio(el, armyName).click();
    await stable();
  }

  beforeEach(() => {
    api = {
      findGame: vi.fn(() => of({ name: 'Alpha', player_count: 1, points_limit: 500 })),
      joinGame: vi.fn(),
      listArmies: vi.fn(() =>
        of({
          armies: [
            armyEntry({ id: 1, name: 'Vorhut', total_cost: 450 }),
            armyEntry({ id: 2, name: 'Hauptmacht', total_cost: 900 }),
            armyEntry({ id: 3, name: 'Leer', unit_count: 0, total_cost: 0 }),
          ],
        }),
      ),
    };
  });

  it('ohne Spielnamen geht es zur Spieleliste', async () => {
    await open('/games/join');
    expect(TestBed.inject(Router).url).toBe('/games/list');
    expect(api.findGame).not.toHaveBeenCalled();
  });

  it('zeigt Spiel und Punktelimit und sperrt zu teure oder leere Armeen', async () => {
    const el = await open('/games/join?name=%20Alpha%20');

    expect(api.findGame).toHaveBeenCalledWith('Alpha');
    expect(text(el)).toContain('Gespielt wird mit 500 Punkten (1/2 Spieler).');
    expect(radio(el, 'Vorhut').disabled).toBe(false);
    expect(radio(el, 'Hauptmacht').disabled).toBe(true);
    expect(text(el)).toContain('Kostet 900 Punkte, erlaubt sind 500.');
    expect(radio(el, 'Leer').disabled).toBe(true);
    expect(text(el)).toContain('Hat noch keine Einheiten.');
  });

  it('laesst auch ohne gefundenes Spiel beitreten', async () => {
    api.findGame.mockReturnValue(throwError(() => new HttpErrorResponse({ status: 500 })));
    const el = await open('/games/join?name=Alpha');

    expect(text(el)).not.toContain('Gespielt wird mit');
    // Ohne Limit ist auch die teure Armee waehlbar.
    expect(radio(el, 'Hauptmacht').disabled).toBe(false);
  });

  it('braucht Passwort und Armee und geht danach in die Lobby', async () => {
    api.joinGame.mockReturnValue(of({}));
    const el = await open('/games/join?name=Alpha');
    expect(submit(el).disabled).toBe(true);

    await fillIn(el, 'geheim', 'Vorhut');
    expect(submit(el).disabled).toBe(false);

    submit(el).click();
    await stable();

    expect(api.joinGame).toHaveBeenCalledWith('Alpha', 'geheim', 1);
    expect(TestBed.inject(Router).url).toBe('/games/lobby');
  });

  it('zeigt die Fehlermeldung bei falschem Passwort', async () => {
    api.joinGame.mockReturnValue(
      throwError(() => new HttpErrorResponse({ status: 403, error: { detail: 'Falsches Passwort.' } })),
    );
    const el = await open('/games/join?name=Alpha');
    await fillIn(el, 'falsch', 'Vorhut');

    submit(el).click();
    await stable();

    expect(el.querySelector('.error')?.textContent).toBe('Falsches Passwort.');
    expect(TestBed.inject(Router).url).toBe('/games/join?name=Alpha');
    expect(submit(el).disabled).toBe(false);
  });
});
