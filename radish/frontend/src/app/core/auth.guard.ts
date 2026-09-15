import { inject } from '@angular/core';
import { CanActivateFn, Router } from '@angular/router';

import { AuthService } from './auth.service';

/** Schuetzt /games und /game/:name -- ohne gueltigen Token geht's zu /login. */
export const authGuard: CanActivateFn = (_route, state) => {
  const auth = inject(AuthService);
  const router = inject(Router);

  if (auth.isAuthenticated()) {
    return true;
  }

  return router.createUrlTree(['/login'], { queryParams: { redirect: state.url } });
};

/**
 * Schuetzt /games/admin zusaetzlich zu authGuard (das an der Elternroute
 * "games" sitzt und hier deshalb schon vorausgesetzt werden kann): nur mit
 * der Keycloak-Realm-Rolle "Radish-Admin" (siehe AuthService.isAdmin) geht
 * es weiter, sonst zurueck zur Spieleliste -- anders als authGuard also
 * nicht zu /login, der Nutzer ist ja bereits angemeldet, ihm fehlt nur die
 * Rolle. Die eigentliche Durchsetzung passiert ohnehin serverseitig (siehe
 * radish/backend/api/permissions.py, HasAdminRole); das hier verhindert nur,
 * dass jemand ohne Rolle ueberhaupt die (dann ohnehin leere/fehlerhafte)
 * Adminseite aufruft.
 */
export const adminGuard: CanActivateFn = () => {
  const auth = inject(AuthService);
  const router = inject(Router);

  if (auth.isAdmin()) {
    return true;
  }

  return router.createUrlTree(['/games/list']);
};
