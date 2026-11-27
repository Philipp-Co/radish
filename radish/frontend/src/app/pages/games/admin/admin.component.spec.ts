import { HttpErrorResponse } from '@angular/common/http';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { Observable, of, throwError } from 'rxjs';

import { ApiService, GameServerEntry } from '../../../core/api.service';
import { AdminComponent } from './admin.component';

function server(overrides: Partial<GameServerEntry> = {}): GameServerEntry {
  return { id: 1, name: 'gs-1', address: 'game-server', port: 7000, is_occupied: false, ...overrides };
}

describe('AdminComponent (Game-Server)', () => {
  let respond: () => Observable<{ servers: GameServerEntry[] }>;
  let listServers: ReturnType<typeof vi.fn>;
  let fixture: ComponentFixture<AdminComponent>;

  async function render(): Promise<HTMLElement> {
    TestBed.configureTestingModule({
      imports: [AdminComponent],
      providers: [{ provide: ApiService, useValue: { listServers } }],
    });
    fixture = TestBed.createComponent(AdminComponent);
    await fixture.whenStable();
    return fixture.nativeElement as HTMLElement;
  }

  async function poll(): Promise<void> {
    vi.advanceTimersByTime(5000);
    await fixture.whenStable();
  }

  const rows = (el: HTMLElement) =>
    Array.from(el.querySelectorAll('tbody tr')).map((tr) =>
      Array.from(tr.querySelectorAll('td')).map((td) => td.textContent?.trim()),
    );

  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['setInterval', 'clearInterval'] });
    respond = () => of({ servers: [server(), server({ id: 2, name: 'gs-2', port: 7001, is_occupied: true })] });
    listServers = vi.fn(() => respond());
  });

  afterEach(() => {
    fixture.destroy();
    vi.useRealTimers();
  });

  it('zeigt die angemeldeten Server mit Status', async () => {
    const el = await render();
    expect(rows(el)).toEqual([
      ['gs-1', 'game-server', '7000', 'frei'],
      ['gs-2', 'game-server', '7001', 'belegt'],
    ]);
  });

  it('pollt alle 5 s', async () => {
    const el = await render();
    respond = () => of({ servers: [] });
    await poll();
    expect(listServers).toHaveBeenCalledTimes(2);
    expect(el.textContent).toContain('Aktuell kein angemeldeter Game-Server.');
  });

  it('laedt auf Knopfdruck sofort neu', async () => {
    const el = await render();
    respond = () => of({ servers: [server({ name: 'gs-neu' })] });

    el.querySelector('button')!.click();
    await fixture.whenStable();

    expect(rows(el)[0][0]).toBe('gs-neu');
  });

  it('meldet einen Ladefehler', async () => {
    respond = () => throwError(() => new HttpErrorResponse({ status: 502 }));
    const el = await render();
    expect(el.querySelector('.error')?.textContent).toBe('Serverliste konnte nicht geladen werden.');
  });

  it('pollt nach einem Fehler weiter und blendet die Meldung wieder aus', async () => {
    respond = () => throwError(() => new HttpErrorResponse({ status: 502 }));
    const el = await render();

    respond = () => of({ servers: [server()] });
    await poll();

    expect(listServers).toHaveBeenCalledTimes(2);
    expect(el.querySelector('.error')).toBeNull();
    expect(rows(el)).toHaveLength(1);
  });

  it('blendet die Meldung nach erfolgreichem Aktualisieren aus', async () => {
    respond = () => throwError(() => new HttpErrorResponse({ status: 502 }));
    const el = await render();

    respond = () => of({ servers: [server()] });
    el.querySelector('button')!.click();
    await fixture.whenStable();

    expect(el.querySelector('.error')).toBeNull();
    expect(rows(el)).toHaveLength(1);
  });
});
