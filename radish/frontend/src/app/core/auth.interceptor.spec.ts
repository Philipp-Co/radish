import { HttpClient, provideHttpClient, withInterceptors } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { TestBed } from '@angular/core/testing';
import { Router } from '@angular/router';
import { firstValueFrom } from 'rxjs';

import { authInterceptor } from './auth.interceptor';
import { AuthService } from './auth.service';

describe('authInterceptor', () => {
  let http: HttpClient;
  let backend: HttpTestingController;
  let auth: { getValidAccessToken: ReturnType<typeof vi.fn>; refreshAccessToken: ReturnType<typeof vi.fn> };
  let navigate: ReturnType<typeof vi.fn>;

  beforeEach(() => {
    auth = {
      getValidAccessToken: vi.fn().mockResolvedValue('access-1'),
      refreshAccessToken: vi.fn().mockResolvedValue('access-2'),
    };
    navigate = vi.fn();
    TestBed.configureTestingModule({
      providers: [
        provideHttpClient(withInterceptors([authInterceptor])),
        provideHttpClientTesting(),
        { provide: AuthService, useValue: auth },
        { provide: Router, useValue: { navigate } },
      ],
    });
    http = TestBed.inject(HttpClient);
    backend = TestBed.inject(HttpTestingController);
  });

  afterEach(() => backend.verify());

  /** Wartet, bis der Interceptor (nach dem Token-Promise) den Request abgeschickt hat. */
  const nextRequest = (url: string) => vi.waitFor(() => backend.expectOne(url));

  it('laesst fremde URLs (z.B. Keycloak) unveraendert', async () => {
    const response = firstValueFrom(http.get('http://localhost:8081/realms/radish/x'));
    const request = backend.expectOne('http://localhost:8081/realms/radish/x');
    expect(request.request.headers.has('Authorization')).toBe(false);
    request.flush({});
    await response;
    expect(auth.getValidAccessToken).not.toHaveBeenCalled();
  });

  it('haengt den Bearer-Token an eigene API-Aufrufe', async () => {
    const response = firstValueFrom(http.get('/api/games/'));
    const request = await nextRequest('/api/games/');
    expect(request.request.headers.get('Authorization')).toBe('Bearer access-1');
    request.flush([]);
    await expect(response).resolves.toEqual([]);
  });

  it('schickt ohne Token ohne Header', async () => {
    auth.getValidAccessToken.mockResolvedValue(null);
    const response = firstValueFrom(http.get('/api/games/'));
    const request = await nextRequest('/api/games/');
    expect(request.request.headers.has('Authorization')).toBe(false);
    request.flush([]);
    await response;
  });

  it('erneuert bei 401 einmal und wiederholt den Request', async () => {
    const response = firstValueFrom(http.get('/api/games/'));
    (await nextRequest('/api/games/')).flush(null, { status: 401, statusText: 'Unauthorized' });

    const retry = await nextRequest('/api/games/');
    expect(retry.request.headers.get('Authorization')).toBe('Bearer access-2');
    retry.flush(['ok']);

    await expect(response).resolves.toEqual(['ok']);
    expect(navigate).not.toHaveBeenCalled();
  });

  it('geht zu /login, wenn auch der Refresh scheitert', async () => {
    auth.refreshAccessToken.mockRejectedValue(new Error('invalid_grant'));
    const response = firstValueFrom(http.get('/api/games/'));
    (await nextRequest('/api/games/')).flush(null, { status: 401, statusText: 'Unauthorized' });

    await expect(response).rejects.toMatchObject({ status: 401 });
    expect(navigate).toHaveBeenCalledWith(['/login']);
  });

  it('reicht andere Fehler ohne Refresh durch', async () => {
    const response = firstValueFrom(http.get('/api/games/'));
    (await nextRequest('/api/games/')).flush(null, { status: 500, statusText: 'Server Error' });

    await expect(response).rejects.toMatchObject({ status: 500 });
    expect(auth.refreshAccessToken).not.toHaveBeenCalled();
  });
});
