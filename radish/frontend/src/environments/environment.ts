/*
 * Entwicklungs-Konfiguration (Default, wird von `ng serve` genutzt --
 * siehe angular.json, "development" ist dort defaultConfiguration fuer
 * serve). apiBaseUrl/wasmBaseUrl bleiben bewusst relative Pfade: sowohl
 * proxy.conf.json (ng serve) als auch radish/backend/web/ (Djangos eigenes
 * Ausliefern, im Container wie im Produktivbetrieb -- es gibt keinen
 * separaten Webserver-Container mehr) binden /api und /wasm unter
 * derselben Origin ein wie die Angular-App selbst -- dadurch braucht die
 * eigene API keine CORS-Sonderbehandlung. Nur Keycloak liegt auf einer
 * anderen Origin (eigener Port), das ist beim OIDC-Redirect-Flow so
 * vorgesehen.
 */
export const environment = {
  production: false,
  apiBaseUrl: '',
  wasmBaseUrl: '/wasm',
  keycloak: {
    // Muss zu KEYCLOAK_BASE_URL/KEYCLOAK_REALM in
    // radish/backend/config/settings.py passen (dieselbe Keycloak-Instanz).
    issuer: 'http://localhost:8081/realms/radish',
    // Oeffentlicher Client mit PKCE (siehe docker/keycloak/realm-radish.json,
    // Client "radish-web") -- kein Client-Secret im Frontend.
    clientId: 'radish-web',
    redirectUri: 'http://localhost:4200/callback',
    postLogoutRedirectUri: 'http://localhost:4200/',
  },
  relay: {
    // Signaling-Server aus radish/relay/server.py -- muss separat laufen
    // (siehe README.md im Repo-Wurzelverzeichnis), ist noch nicht Teil von
    // docker-compose.yaml. Bezieht sich auf genau eine Spielserver-Instanz
    // (--udp-host/--udp-port beim Start von relay/server.py): welches
    // Game man im Game-Browser erstellt/beitritt, aendert daran aktuell
    // nichts, da GameDetailSerializer den zugewiesenen Server bewusst nicht
    // nach aussen gibt (siehe radish/backend/api/serializers.py).
    signalingUrl: 'ws://localhost:8765',
  },
};
