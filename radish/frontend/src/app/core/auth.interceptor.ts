import { HttpInterceptorFn } from '@angular/common/http';
import { inject } from '@angular/core';
import { Router } from '@angular/router';
import { catchError, from, switchMap, throwError } from 'rxjs';

import { AuthService } from './auth.service';

/**
 * Haengt bei eigenen API-Aufrufen (relative Pfade unter /api/, siehe
 * ApiService) den Bearer-Access-Token an. Keycloak-Aufrufe laufen ueber
 * eine absolute URL auf einer anderen Origin (siehe AuthService) und
 * durchlaufen deshalb nie diesen Zweig -- dort ist der Access-Token ja
 * gerade erst das Ergebnis des Aufrufs, nicht seine Voraussetzung.
 *
 * Der Token kommt aus getValidAccessToken(): ist er abgelaufen oder laeuft
 * gleich ab, wird er vor dem Request erneuert. Das ist der Normalfall nach
 * einem Hintergrund-Tab oder Ruhezustand, in dem der Refresh-Timer in
 * AuthService nicht rechtzeitig dran war.
 *
 * Bei 401 trotzdem (etwa weil Keycloak den Token frueher verworfen hat) wird
 * einmalig per Refresh-Token ein neuer Token geholt und der Request
 * wiederholt -- auch dann, wenn vorher gar kein gueltiger Token da war.
 * Schlaegt auch das fehl, geht es zu /login.
 */
export const authInterceptor: HttpInterceptorFn = (req, next) => {
  const auth = inject(AuthService);
  const router = inject(Router);

  if (!req.url.startsWith('/api/')) {
    return next(req);
  }

  const withToken = (token: string | null) =>
    token ? req.clone({ setHeaders: { Authorization: `Bearer ${token}` } }) : req;

  return from(auth.getValidAccessToken()).pipe(
    switchMap((token) => next(withToken(token))),
    catchError((err) => {
      if (err?.status !== 401) {
        return throwError(() => err);
      }
      return from(auth.refreshAccessToken().catch(() => null)).pipe(
        switchMap((newToken) => {
          if (!newToken) {
            router.navigate(['/login']);
            return throwError(() => err);
          }
          return next(withToken(newToken));
        }),
      );
    }),
  );
};
