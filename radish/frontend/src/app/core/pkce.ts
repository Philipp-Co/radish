/*
 * PKCE-Hilfsfunktionen (RFC 7636) fuer den Authorization-Code-Flow des
 * oeffentlichen Keycloak-Clients "radish-web" (siehe
 * docker/keycloak/realm-radish.json: "publicClient": true,
 * "pkce.code.challenge.method": "S256"). Bewusst ohne externe
 * OIDC-Bibliothek -- die Web Crypto API im Browser reicht fuer die paar
 * Schritte hier (Zufallswerte, SHA-256, Base64url), und ein Client ohne
 * Secret braucht keine komplexere Bibliothek, um sicher zu sein.
 */

function base64UrlEncode(bytes: Uint8Array): string {
  let binary = '';
  for (const byte of bytes) {
    binary += String.fromCharCode(byte);
  }
  return btoa(binary).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

function randomBytes(length: number): Uint8Array {
  const bytes = new Uint8Array(length);
  crypto.getRandomValues(bytes);
  return bytes;
}

/** Zufaelliger state-Wert gegen CSRF beim Redirect-Rueckweg. */
export function generateState(): string {
  return base64UrlEncode(randomBytes(32));
}

/** code_verifier: 43-128 Zeichen aus [A-Za-z0-9-._~], siehe RFC 7636 Section 4.1. */
export function generateCodeVerifier(): string {
  return base64UrlEncode(randomBytes(64));
}

/** code_challenge = BASE64URL(SHA256(code_verifier)), siehe RFC 7636 Section 4.2. */
export async function deriveCodeChallenge(codeVerifier: string): Promise<string> {
  const encoded = new TextEncoder().encode(codeVerifier);
  const digest = await crypto.subtle.digest('SHA-256', encoded);
  return base64UrlEncode(new Uint8Array(digest));
}
