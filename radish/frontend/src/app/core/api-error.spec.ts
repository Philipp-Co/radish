import { HttpErrorResponse } from '@angular/common/http';

import { apiErrorMessage } from './api-error';

const errorWith = (error: unknown) => new HttpErrorResponse({ status: 400, error });

describe('apiErrorMessage', () => {
  it('liest {"detail": ...} der Matchmaking-Endpunkte', () => {
    expect(apiErrorMessage(errorWith({ detail: 'Spiel ist voll.' }), 'Fehler')).toBe(
      'Spiel ist voll.',
    );
  });

  it('findet die erste Meldung in verschachtelten DRF-Fehlern', () => {
    const body = { units: [{}, { entities: [{ weapon_ids: ['Waffe passt nicht.'] }] }] };
    expect(apiErrorMessage(errorWith(body), 'Fehler')).toBe('Waffe passt nicht.');
  });

  it('nimmt bei einer Liste auf oberster Ebene den ersten String', () => {
    expect(apiErrorMessage(errorWith(['Erst das.', 'Dann das.']), 'Fehler')).toBe('Erst das.');
  });

  it('stellt die Feldbezeichnung voran, wenn es eine gibt', () => {
    const body = { cost: ['Muss positiv sein.'] };
    expect(apiErrorMessage(errorWith(body), 'Fehler', { cost: 'Kosten' })).toBe(
      'Kosten: Muss positiv sein.',
    );
  });

  it('ueberspringt Felder ohne Meldung', () => {
    const body = { name: [], cost: ['Zu hoch.'] };
    expect(apiErrorMessage(errorWith(body), 'Fehler')).toBe('Zu hoch.');
  });

  it.each([
    ['Text-Body (z.B. Djangos HTML-Fehlerseite)', '<html>Server Error</html>'],
    ['kein Body', null],
    ['Objekt ohne Strings', { detail: { code: 3 } }],
  ])('faellt zurueck bei %s', (_name, body) => {
    expect(apiErrorMessage(errorWith(body), 'Fehler')).toBe('Fehler');
  });
});
