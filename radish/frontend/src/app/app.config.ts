import { ApplicationConfig, provideZonelessChangeDetection } from '@angular/core';
import { provideRouter } from '@angular/router';
import { provideHttpClient, withInterceptors } from '@angular/common/http';

import { routes } from './app.routes';
import { authInterceptor } from './core/auth.interceptor';

export const appConfig: ApplicationConfig = {
  providers: [
    // Ohne zone.js: Angular zeichnet neu, wenn sich ein Signal aendert, ein
    // Template-Ereignis oder ein Output feuert. Zustand, der sich asynchron
    // aendert (HTTP-Antworten, Polling, Timer), muss deshalb in einem Signal
    // stehen -- ein Feld, das nur in einem subscribe() gesetzt wird, erschiene
    // sonst erst beim naechsten Neuzeichnen. zone.js ist deshalb auch keine
    // Abhaengigkeit mehr (package.json).
    provideZonelessChangeDetection(),
    provideRouter(routes),
    provideHttpClient(withInterceptors([authInterceptor])),
  ],
};
