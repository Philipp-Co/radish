import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';

export interface GameListEntry {
  name: string;
  player_count: number;
}

export interface GameDetail {
  id: number;
  name: string;
  host_identifier: string;
  second_player_identifier: string | null;
}

/** Ein registrierter Game-Server, siehe radish/backend/api/admin_views.py. */
export interface GameServerEntry {
  id: number;
  name: string;
  ip_address: string;
  port: number;
  is_occupied: boolean;
}

/**
 * Duenner Wrapper um die Matchmaking-Endpunkte unter api/client/ (siehe
 * radish/backend/api/client_views.py, urls.py). command/, stream/ und
 * state/ existieren serverseitig zwar schon (teils noch als Platzhalter,
 * siehe dortige Docstrings), sind hier aber bewusst noch nicht angebunden:
 * das eigentliche Ingame-Geschehen laeuft aktuell ueber den WASM-Client
 * direkt per WebRTC/Relay (siehe shared/game-canvas/GameCanvasComponent), nicht ueber diese
 * REST-Endpunkte -- das ist Aufgabe eines spaeteren Schritts, sobald das
 * Server-Protokoll dafuer steht.
 */
@Injectable({ providedIn: 'root' })
export class ApiService {
  constructor(private readonly http: HttpClient) {}

  listGames(): Observable<{ games: GameListEntry[] }> {
    return this.http.get<{ games: GameListEntry[] }>('/api/client/games/');
  }

  createGame(name: string, password: string): Observable<GameDetail> {
    return this.http.post<GameDetail>('/api/client/game/', { name, password });
  }

  joinGame(name: string, password: string): Observable<GameDetail> {
    return this.http.post<GameDetail>('/api/client/game/join/', { name, password });
  }

  /** Das laufende Spiel des angemeldeten Nutzers, oder `null` wenn er gerade keins hat. */
  getCurrentGame(): Observable<{ game: GameDetail | null }> {
    return this.http.get<{ game: GameDetail | null }>('/api/client/game/current/');
  }

  /** Verlaesst das aktuelle Spiel, siehe radish/backend/api/client_views.py, LeaveGameView. */
  leaveGame(): Observable<{ status: string }> {
    return this.http.post<{ status: string }>('/api/client/game/leave/', {});
  }

  /**
   * Alle aktuell angemeldeten Game-Server (Adminbereich, siehe
   * radish/backend/api/admin_views.py, AdminServerListView -- erfordert die
   * Keycloak-Realm-Rolle "Radish-Admin").
   */
  listServers(): Observable<{ servers: GameServerEntry[] }> {
    return this.http.get<{ servers: GameServerEntry[] }>('/api/admin/servers/');
  }
}
