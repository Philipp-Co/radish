export const environment = {
  production: true,
  apiBaseUrl: '',
  wasmBaseUrl: '/wasm',
  keycloak: {
    issuer: 'http://localhost:8081/realms/radish',
    clientId: 'radish-web',
    // Passend zu redirectUris/webOrigins von "radish-web" in
    // docker/keycloak/realm-radish.json und zum "backend"-Service in
    // docker-compose.yaml (Django liefert das Frontend jetzt selbst aus,
    // kein eigener nginx-Container mehr davor -- siehe radish/backend/web/).
    redirectUri: 'http://localhost:8000/callback',
    postLogoutRedirectUri: 'http://localhost:8000/',
  },
  relay: {
    signalingUrl: 'ws://localhost:8765',
  },
};
