import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { TestBed } from '@angular/core/testing';

import { AuthService } from './auth.service';

const TOKEN_URL = 'http://localhost:8081/realms/radish/protocol/openid-connect/token';
const TOKENS_KEY = 'radish.auth.tokens';
const PENDING_KEY = 'radish.auth.pending';

/** Ein unsigniertes JWT mit diesem Payload -- mehr liest AuthService nicht aus. */
function jwt(payload: object): string {
  const bytes = new TextEncoder().encode(JSON.stringify(payload));
  const base64 = btoa(String.fromCharCode(...bytes));
  const base64url = base64.replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
  return `header.${base64url}.signature`;
}

function storeTokens(tokens: { accessToken: string; refreshToken?: string | null; expiresInMs: number }) {
  sessionStorage.setItem(
    TOKENS_KEY,
    JSON.stringify({
      accessToken: tokens.accessToken,
      refreshToken: tokens.refreshToken ?? 'refresh-1',
      idToken: null,
      expiresAt: Date.now() + tokens.expiresInMs,
    }),
  );
}

describe('AuthService', () => {
  let http: HttpTestingController;

  function create(): AuthService {
    TestBed.configureTestingModule({
      providers: [provideHttpClient(), provideHttpClientTesting()],
    });
    http = TestBed.inject(HttpTestingController);
    return TestBed.inject(AuthService);
  }

  beforeEach(() => {
    sessionStorage.clear();
    // Der Refresh-Timer soll nur feuern, wenn ein Test ihn vorspult.
    vi.useFakeTimers({ toFake: ['setTimeout', 'clearTimeout'] });
  });

  afterEach(() => {
    http.verify();
    vi.useRealTimers();
  });

  describe('Anmeldestatus und Rollen', () => {
    it('ist ohne gespeicherte Tokens abgemeldet', () => {
      const auth = create();
      expect(auth.isAuthenticated()).toBe(false);
      expect(auth.roles()).toEqual([]);
      expect(auth.displayName()).toBeNull();
    });

    it('liest Rollen und Namen (mit Umlauten) aus dem Access-Token', () => {
      storeTokens({
        accessToken: jwt({ realm_access: { roles: ['Player', 'Radish-Admin'] }, given_name: 'Jürgen' }),
        expiresInMs: 3_600_000,
      });
      const auth = create();

      expect(auth.isAuthenticated()).toBe(true);
      expect(auth.roles()).toEqual(['Player', 'Radish-Admin']);
      expect(auth.isAdmin()).toBe(true);
      expect(auth.displayName()).toBe('Jürgen');
    });

    it('faellt beim Namen auf preferred_username zurueck', () => {
      storeTokens({ accessToken: jwt({ preferred_username: 'test-user' }), expiresInMs: 3_600_000 });
      expect(create().displayName()).toBe('test-user');
    });

    it('verkraftet einen kaputten Token ohne Ausnahme', () => {
      storeTokens({ accessToken: 'kein.jwt!.hier', expiresInMs: 3_600_000 });
      const auth = create();
      expect(auth.roles()).toEqual([]);
      expect(auth.isAdmin()).toBe(false);
    });

    it('gilt mit abgelaufenem Token als abgemeldet', () => {
      storeTokens({ accessToken: jwt({}), expiresInMs: -1000 });
      const auth = create();
      expect(auth.isAuthenticated()).toBe(false);
      expect(auth.getAccessToken()).toBeNull();
    });
  });

  describe('getValidAccessToken', () => {
    it('gibt einen noch lange gueltigen Token ohne Refresh zurueck', async () => {
      storeTokens({ accessToken: 'access-1', expiresInMs: 3_600_000 });
      await expect(create().getValidAccessToken()).resolves.toBe('access-1');
    });

    it('erneuert einen gleich ablaufenden Token und teilt sich den Refresh', async () => {
      storeTokens({ accessToken: 'access-1', expiresInMs: 10_000 });
      const auth = create();

      const first = auth.getValidAccessToken();
      const second = auth.getValidAccessToken();

      const request = http.expectOne(TOKEN_URL);
      const body = new URLSearchParams(request.request.body as string);
      expect(body.get('grant_type')).toBe('refresh_token');
      expect(body.get('refresh_token')).toBe('refresh-1');
      request.flush({ access_token: 'access-2', expires_in: 300, token_type: 'Bearer' });

      await expect(first).resolves.toBe('access-2');
      await expect(second).resolves.toBe('access-2');
      expect(auth.getAccessToken()).toBe('access-2');
      // Ohne neuen Refresh-Token in der Antwort bleibt der alte gueltig.
      expect(JSON.parse(sessionStorage.getItem(TOKENS_KEY)!).refreshToken).toBe('refresh-1');
    });

    it('ist ohne Anmeldung null, ohne Keycloak zu fragen', async () => {
      await expect(create().getValidAccessToken()).resolves.toBeNull();
    });
  });

  it('meldet ab, wenn Keycloak den Refresh-Token im Timer ablehnt', async () => {
    storeTokens({ accessToken: 'access-1', expiresInMs: 3_600_000 });
    const auth = create();

    vi.advanceTimersByTime(3_600_000);
    http
      .expectOne(TOKEN_URL)
      .flush({ error: 'invalid_grant' }, { status: 400, statusText: 'Bad Request' });
    await vi.waitFor(() => expect(auth.isAuthenticated()).toBe(false));

    expect(sessionStorage.getItem(TOKENS_KEY)).toBeNull();
  });

  it('versucht es nach einem Netzfehler im Timer spaeter noch einmal', async () => {
    storeTokens({ accessToken: 'access-1', expiresInMs: 3_600_000 });
    const auth = create();

    vi.advanceTimersByTime(3_600_000);
    http.expectOne(TOKEN_URL).error(new ProgressEvent('error'));
    expect(auth.getAccessToken()).toBe('access-1');

    // Der neue Versuch wird erst geplant, wenn der Fehler durch die
    // Promise-Kette ist -- so lange weiter vorspulen.
    const retry = await vi.waitFor(() => {
      vi.advanceTimersByTime(10_000);
      return http.expectOne(TOKEN_URL);
    });
    retry.flush({ access_token: 'access-2', expires_in: 300, token_type: 'Bearer' });
    await vi.waitFor(() => expect(auth.getAccessToken()).toBe('access-2'));
  });

  describe('handleCallback', () => {
    beforeEach(() => {
      sessionStorage.setItem(
        PENDING_KEY,
        JSON.stringify({ codeVerifier: 'verifier', state: 'state-1', redirectAfter: '/games/armies' }),
      );
    });

    it('tauscht den Code gegen Tokens und nennt das Ziel nach dem Login', async () => {
      const auth = create();
      const result = auth.handleCallback(new URLSearchParams({ state: 'state-1', code: 'code-1' }));

      const request = http.expectOne(TOKEN_URL);
      const body = new URLSearchParams(request.request.body as string);
      expect(body.get('grant_type')).toBe('authorization_code');
      expect(body.get('code')).toBe('code-1');
      expect(body.get('code_verifier')).toBe('verifier');
      request.flush({ access_token: jwt({}), refresh_token: 'r', expires_in: 300, token_type: 'Bearer' });

      await expect(result).resolves.toBe('/games/armies');
      expect(auth.isAuthenticated()).toBe(true);
      expect(sessionStorage.getItem(PENDING_KEY)).toBeNull();
    });

    it('lehnt einen fremden state ab', async () => {
      const auth = create();
      await expect(
        auth.handleCallback(new URLSearchParams({ state: 'anders', code: 'code-1' })),
      ).rejects.toThrow('state');
    });

    it('reicht einen Keycloak-Fehler weiter', async () => {
      const auth = create();
      await expect(
        auth.handleCallback(new URLSearchParams({ error: 'access_denied' })),
      ).rejects.toThrow('access_denied');
    });

    it('scheitert ohne laufenden Login', async () => {
      sessionStorage.removeItem(PENDING_KEY);
      const auth = create();
      await expect(
        auth.handleCallback(new URLSearchParams({ state: 'state-1', code: 'c' })),
      ).rejects.toThrow('Kein laufender Login');
    });
  });
});
