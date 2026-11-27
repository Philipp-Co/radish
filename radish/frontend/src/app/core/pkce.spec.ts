import { deriveCodeChallenge, generateCodeVerifier, generateState } from './pkce';

const BASE64URL = /^[A-Za-z0-9_-]+$/;

describe('pkce', () => {
  it('erzeugt einen code_verifier nach RFC 7636 (43-128 Zeichen, base64url)', () => {
    const verifier = generateCodeVerifier();
    expect(verifier).toMatch(BASE64URL);
    expect(verifier.length).toBeGreaterThanOrEqual(43);
    expect(verifier.length).toBeLessThanOrEqual(128);
  });

  it('erzeugt bei jedem Aufruf einen anderen state', () => {
    const a = generateState();
    const b = generateState();
    expect(a).toMatch(BASE64URL);
    expect(a).not.toBe(b);
  });

  it('leitet die code_challenge aus dem Beispiel in RFC 7636 Anhang B ab', async () => {
    const challenge = await deriveCodeChallenge('dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk');
    expect(challenge).toBe('E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM');
  });
});
