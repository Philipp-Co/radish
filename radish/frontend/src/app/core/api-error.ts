import { HttpErrorResponse } from '@angular/common/http';

/**
 * Erste lesbare Fehlermeldung aus einer API-Antwort. Die Matchmaking-
 * Endpunkte antworten mit {"detail": "..."}, Validierungsfehler von DRF
 * dagegen je Feld und verschachtelt (z.B. {"non_field_errors": ["..."]}
 * oder {"units": [{"entities": [...]}]}) -- deshalb wird rekursiv nach dem
 * ersten String gesucht. Nur JSON-Antworten zaehlen: ein Text-Body waere
 * z.B. Djangos HTML-Fehlerseite.
 *
 * Mit fieldLabels wird einer Meldung, die an einem Feld der obersten Ebene
 * haengt, dessen Bezeichnung vorangestellt ("Kosten: ...") -- sonst bliebe
 * bei Formularen mit vielen Feldern offen, welches gemeint ist.
 */
export function apiErrorMessage(
  err: HttpErrorResponse,
  fallback: string,
  fieldLabels: Record<string, string> = {},
): string {
  const body: unknown = err.error;
  if (Array.isArray(body)) {
    return firstString(body) ?? fallback;
  }
  if (!body || typeof body !== 'object') {
    return fallback;
  }
  for (const [field, value] of Object.entries(body)) {
    const message = firstString(value);
    if (message) {
      const label = fieldLabels[field];
      return label ? `${label}: ${message}` : message;
    }
  }
  return fallback;
}

function firstString(value: unknown): string | null {
  if (typeof value === 'string') {
    return value;
  }
  if (Array.isArray(value)) {
    for (const entry of value) {
      const found = firstString(entry);
      if (found) {
        return found;
      }
    }
    return null;
  }
  if (value && typeof value === 'object') {
    return firstString(Object.values(value));
  }
  return null;
}
