import { Injectable, computed, signal } from '@angular/core';
import { HttpClient, HttpParams } from '@angular/common/http';
import { firstValueFrom } from 'rxjs';

import { environment } from '../../environments/environment';
import { deriveCodeChallenge, generateCodeVerifier, generateState } from './pkce';

interface StoredTokens {
  accessToken: string;
  refreshToken: string | null;
  idToken: string | null;
  /** Epoch-Millisekunden, ab wann der Access-Token nicht mehr gueltig ist. */
  expiresAt: number;
}

interface PendingLogin {
  codeVerifier: string;
  state: string;
  redirectAfter: string;
}

interface TokenResponse {
  access_token: string;
  refresh_token?: string;
  id_token?: string;
  expires_in: number;
  token_type: string;
}

// sessionStorage statt localStorage: Tokens sollen nicht ueber das Schliessen
// des Tabs hinaus liegen bleiben. Ein XSS im Frontend koennte sie trotzdem
// auslesen -- das ist der bekannte Kompromiss eines oeffentlichen SPA-Clients
// ohne eigenes Backend-Cookie (siehe docker/keycloak/realm-radish.json,
// "radish-web" ist bewusst "publicClient": true, kein Client-Secret hier).
const TOKENS_KEY = 'radish.auth.tokens';
const PENDING_KEY = 'radish.auth.pending';

/**
 * Dekodiert den JWT-Payload (Base64url, per Definition kein valides
 * Base64 -- "-"/"_" statt "+"/"/", ohne Padding) und liest daraus
 * "realm_access.roles". atob() gibt Latin1-Bytes zurueck, deshalb der
 * Umweg ueber encodeURIComponent/decodeURIComponent fuer korrektes UTF-8
 * (Namen/Rollen koennen Umlaute o.ae. enthalten). Bei fehlendem/kaputtem
 * Token oder Payload wird bewusst still [] zurueckgegeben statt zu werfen --
 * das hier ist nur eine UI-Anzeigeentscheidung, kein Sicherheitscheck
 * (siehe Kommentar bei AuthService.roles).
 */
function decodeRealmRoles(accessToken: string | null): string[] {
  if (!accessToken) {
    return [];
  }
  const payloadSegment = accessToken.split('.')[1];
  if (!payloadSegment) {
    return [];
  }
  try {
    const normalized = payloadSegment.replace(/-/g, '+').replace(/_/g, '/');
    const padded = normalized.padEnd(normalized.length + ((4 - (normalized.length % 4)) % 4), '=');
    const json = decodeURIComponent(
      atob(padded)
        .split('')
        .map((char) => '%' + char.charCodeAt(0).toString(16).padStart(2, '0'))
        .join(''),
    );
    const claims = JSON.parse(json) as { realm_access?: { roles?: string[] } };
    return claims.realm_access?.roles ?? [];
  } catch {
    return [];
  }
}

/**
 * Authorization-Code-Flow mit PKCE gegen den Keycloak-Client "radish-web"
 * (siehe pkce.ts). Anders als radish/backend/api/auth_views.py (der
 * serverseitige, session-basierte Flow des confidential Clients
 * "radish-backend") bekommt der Browser hier den Access-Token direkt und
 * schickt ihn als Bearer-Token an die Django-API (siehe auth.interceptor.ts),
 * die ihn per KeycloakJWTAuthentication validiert (radish/backend/api/
 * authentication.py).
 */
@Injectable({ providedIn: 'root' })
export class AuthService {
  private readonly tokens = signal<StoredTokens | null>(this.loadTokens());
  private refreshTimer: ReturnType<typeof setTimeout> | null = null;

  /**
   * Liest ausschliesslich vom zuletzt bekannten expiresAt ab -- laeuft der
   * Token zwischen zwei Aenderungen von tokens() im Hintergrund ab, bleibt
   * dieser Wert bis zur naechsten Aktualisierung (Refresh oder Logout)
   * optimistisch "true". scheduleRefresh() erneuert den Token aber schon
   * vor Ablauf, sodass das in der Praxis nicht auffaellt; fuer eine harte
   * Garantie muesste man stattdessen bei jedem Zugriff die Zeit pruefen statt
   * sich auf ein computed() zu verlassen.
   */
  readonly isAuthenticated = computed(() => {
    const current = this.tokens();
    return !!current && current.expiresAt > Date.now();
  });

  /**
   * Keycloak-Realm-Rollen aus dem Access-Token (Klaim "realm_access.roles"),
   * z.B. "Player" oder "Radish-Admin" (siehe docker/keycloak/realm-radish.json).
   * Der Access-Token ist ein JWT, dessen Payload (mittlerer Teil) reines,
   * unverschluesseltes Base64url-JSON ist -- fuer eine rein clientseitige
   * Anzeigeentscheidung (z.B. den Admin-Tab in games-shell.component.ts) reicht
   * das Auslesen hier, ohne Signaturpruefung: durchgesetzt wird die Rolle
   * ohnehin serverseitig (siehe radish/backend/api/permissions.py,
   * HasAdminRole), das hier entscheidet nur ueber die UI-Sichtbarkeit.
   */
  readonly roles = computed(() => decodeRealmRoles(this.tokens()?.accessToken ?? null));

  /** Ob der angemeldete Nutzer die Keycloak-Realm-Rolle "Radish-Admin" hat. */
  readonly isAdmin = computed(() => this.roles().includes('Radish-Admin'));

  constructor(private readonly http: HttpClient) {
    const current = this.tokens();
    if (current) {
      this.scheduleRefresh(current.expiresAt);
    }
  }

  private loadTokens(): StoredTokens | null {
    const raw = sessionStorage.getItem(TOKENS_KEY);
    if (!raw) {
      return null;
    }
    try {
      return JSON.parse(raw) as StoredTokens;
    } catch {
      return null;
    }
  }

  private storeTokens(tokens: StoredTokens): void {
    sessionStorage.setItem(TOKENS_KEY, JSON.stringify(tokens));
    this.tokens.set(tokens);
    this.scheduleRefresh(tokens.expiresAt);
  }

  private clearTokens(): void {
    sessionStorage.removeItem(TOKENS_KEY);
    this.tokens.set(null);
    if (this.refreshTimer !== null) {
      clearTimeout(this.refreshTimer);
      this.refreshTimer = null;
    }
  }

  private scheduleRefresh(expiresAt: number): void {
    if (this.refreshTimer !== null) {
      clearTimeout(this.refreshTimer);
    }
    // 30s Sicherheitsabstand vor dem tatsaechlichen Ablauf, mindestens aber
    // 5s (falls Keycloak einmal eine sehr kurze Lebensdauer konfiguriert hat).
    const refreshInMs = Math.max(expiresAt - Date.now() - 30_000, 5_000);
    this.refreshTimer = setTimeout(() => {
      this.refreshAccessToken().catch(() => {
        // Refresh fehlgeschlagen (z.B. auch der Refresh-Token abgelaufen) --
        // lokale Sitzung beenden; der authGuard schickt beim naechsten
        // Routenwechsel zu /login.
        this.clearTokens();
      });
    }, refreshInMs);
  }

  getAccessToken(): string | null {
    const current = this.tokens();
    if (!current || current.expiresAt <= Date.now()) {
      return null;
    }
    return current.accessToken;
  }

  /**
   * Startet den Login-Redirect zu Keycloak. redirectAfter ist der Pfad
   * innerhalb der App, zu dem nach erfolgreichem Login weitergeleitet wird
   * (siehe authGuard, der ihn beim Umleiten zu /login als Query-Parameter
   * "redirect" mitgibt).
   */
  async login(redirectAfter = '/games'): Promise<void> {
    const codeVerifier = generateCodeVerifier();
    const state = generateState();
    const codeChallenge = await deriveCodeChallenge(codeVerifier);

    const pending: PendingLogin = { codeVerifier, state, redirectAfter };
    sessionStorage.setItem(PENDING_KEY, JSON.stringify(pending));

    const params = new URLSearchParams({
      client_id: environment.keycloak.clientId,
      response_type: 'code',
      scope: 'openid',
      redirect_uri: environment.keycloak.redirectUri,
      state,
      code_challenge: codeChallenge,
      code_challenge_method: 'S256',
    });

    window.location.href = `${environment.keycloak.issuer}/protocol/openid-connect/auth?${params.toString()}`;
  }

  /**
   * Verarbeitet den Rueckweg von Keycloak (siehe CallbackComponent). Prueft
   * den state-Parameter gegen den vor dem Redirect gemerkten Wert, tauscht
   * den Code gegen Tokens und gibt den Pfad zurueck, zu dem jetzt
   * weitergeleitet werden soll.
   */
  async handleCallback(queryParams: URLSearchParams): Promise<string> {
    const error = queryParams.get('error');
    if (error) {
      throw new Error(`Keycloak meldet einen Fehler: ${error}`);
    }

    const pendingRaw = sessionStorage.getItem(PENDING_KEY);
    if (!pendingRaw) {
      throw new Error('Kein laufender Login-Vorgang gefunden (Session abgelaufen?).');
    }
    sessionStorage.removeItem(PENDING_KEY);
    const pending = JSON.parse(pendingRaw) as PendingLogin;

    const state = queryParams.get('state');
    if (!state || state !== pending.state) {
      throw new Error('Ungueltiger oder fehlender state-Parameter.');
    }

    const code = queryParams.get('code');
    if (!code) {
      throw new Error('Kein code-Parameter in der Antwort von Keycloak.');
    }

    await this.exchangeCode(code, pending.codeVerifier);
    return pending.redirectAfter;
  }

  private async exchangeCode(code: string, codeVerifier: string): Promise<void> {
    const body = new HttpParams()
      .set('grant_type', 'authorization_code')
      .set('client_id', environment.keycloak.clientId)
      .set('code', code)
      .set('redirect_uri', environment.keycloak.redirectUri)
      .set('code_verifier', codeVerifier);

    const response = await firstValueFrom(
      this.http.post<TokenResponse>(
        `${environment.keycloak.issuer}/protocol/openid-connect/token`,
        body.toString(),
        { headers: { 'Content-Type': 'application/x-www-form-urlencoded' } },
      ),
    );
    this.applyTokenResponse(response);
  }

  /**
   * Erneuert den Access-Token per refresh_token-Grant (kein erneuter
   * Redirect noetig). Wird sowohl vom Timer aus scheduleRefresh() als auch
   * vom auth.interceptor.ts bei einer 401-Antwort aufgerufen.
   */
  async refreshAccessToken(): Promise<string | null> {
    const current = this.tokens();
    if (!current?.refreshToken) {
      return null;
    }

    const body = new HttpParams()
      .set('grant_type', 'refresh_token')
      .set('client_id', environment.keycloak.clientId)
      .set('refresh_token', current.refreshToken);

    const response = await firstValueFrom(
      this.http.post<TokenResponse>(
        `${environment.keycloak.issuer}/protocol/openid-connect/token`,
        body.toString(),
        { headers: { 'Content-Type': 'application/x-www-form-urlencoded' } },
      ),
    );
    this.applyTokenResponse(response);
    return response.access_token;
  }

  private applyTokenResponse(response: TokenResponse): void {
    this.storeTokens({
      accessToken: response.access_token,
      refreshToken: response.refresh_token ?? null,
      idToken: response.id_token ?? null,
      expiresAt: Date.now() + response.expires_in * 1000,
    });
  }

  /** Beendet zuerst die lokale Sitzung, dann (per Redirect) die Keycloak-SSO-Session. */
  logout(): void {
    const idToken = this.tokens()?.idToken ?? undefined;
    this.clearTokens();

    const params = new URLSearchParams({
      post_logout_redirect_uri: environment.keycloak.postLogoutRedirectUri,
    });
    if (idToken) {
      params.set('id_token_hint', idToken);
    } else {
      params.set('client_id', environment.keycloak.clientId);
    }
    window.location.href = `${environment.keycloak.issuer}/protocol/openid-connect/logout?${params.toString()}`;
  }
}
