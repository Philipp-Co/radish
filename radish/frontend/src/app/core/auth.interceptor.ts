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
 * Bei 401 (Access-Token zwischenzeitlich abgelaufen, z.B. weil der
 * Refresh-Timer in AuthService noch nicht dran war) wird einmalig per
 * Refresh-Token ein neuer Token geholt und der Request wiederholt --
 * schlaegt auch das fehl, geht es zu /login.
 */
export const authInterceptor: HttpInterceptorFn = (req, next) => {
  const auth = inject(AuthService);
  const router = inject(Router);

  if (!req.url.startsWith('/api/')) {
    return next(req);
  }

  const token = auth.getAccessToken();
  const authorizedReq = token
    ? req.clone({ setHeaders: { Authorization: `Bearer ${token}` } })
    : req;

  return next(authorizedReq).pipe(
    catchError((err) => {
      if (err?.status !== 401 || !token) {
        return throwError(() => err);
      }
      return from(auth.refreshAccessToken()).pipe(
        switchMap((newToken) => {
          if (!newToken) {
            router.navigate(['/login']);
            return throwError(() => err);
          }
          const retriedReq = req.clone({ setHeaders: { Authorization: `Bearer ${newToken}` } });
          return next(retriedReq);
        }),
      );
    }),
  );
};
