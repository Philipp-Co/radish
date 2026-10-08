import { Injectable, inject } from '@angular/core';

import { AuthService } from './auth.service';

/**
 * Callback, an den GameSocketService die dekodierten Bytes einer
 * "event"-Nachricht reicht (siehe handleMessage() unten) -- von
 * GameCanvasComponent registriert, sobald das WASM-Modul geladen ist.
 */
export type WasmEventHandler = (data: Uint8Array) => void;

/**
 * Callback, an den GameSocketService meldet, ob die Verbindung gerade offen
 * ist -- von GameCanvasComponent registriert und dort an
 * zuc_on_connection_state() (client/src/main.c) weitergereicht.
 */
export type ConnectionStateHandler = (open: boolean) => void;
export type PlayerIdHandler = (playerId: string) => void;

/**
 * Wie oft der Client dem Backend ein Lebenszeichen schickt, waehrend die
 * Verbindung offen ist (siehe startHeartbeat() unten) -- derselbe Wert wie
 * PING_INTERVAL_SECONDS in radish/backend/api/consumers.py, EchoConsumer,
 * das dieselbe Idee bereits in die andere Richtung (Server -> Client)
 * umsetzt. Kein gemeinsam importierter Wert, weil Frontend und Backend
 * getrennte Projekte/Sprachen sind -- bei einer Aenderung dort also auch
 * hier nachziehen.
 */
const HEARTBEAT_INTERVAL_MS = 10_000;

/**
 * Oeffnet die WebSocket-Verbindung zum Backend (siehe radish/backend/api/
 * consumers.py, EchoConsumer -- aktuell die UDP-Bruecke zum Spielserver des
 * laufenden Spiels). Der Access-Token kommt als Query-Parameter statt als
 * Header: die Browser-WebSocket-API erlaubt beim Handshake keine eigenen
 * Header (siehe radish/backend/api/middleware.py, KeycloakTokenAuthMiddleware,
 * die genau diesen Query-Parameter erwartet).
 *
 * Schickt, solange die Verbindung offen ist, alle HEARTBEAT_INTERVAL_MS ein
 * eigenes Lebenszeichen ans Backend (siehe startHeartbeat()) -- das
 * Gegenstueck zum Ping, den das Backend schon in die andere Richtung
 * schickt (_ping_loop in consumers.py). connect()/disconnect() werden nur
 * von CurrentGameComponent aufgerufen (siehe dort), solange dort ein
 * laufendes Spiel angezeigt wird -- der Heartbeat existiert dadurch von
 * selbst nur auf der Seite "Aktuelles Spiel" und nur bei offener Verbindung,
 * ohne dass dieser Service selbst wissen muesste, auf welcher Route er
 * gerade laeuft.
 *
 * Nachrichten sind JSON (siehe radish/backend/api/consumers.py): ein
 * "type"-Feld unterscheidet sie. Beim Typ "event" enthaelt "data" die
 * eigentlichen Spieldaten Base64-kodiert -- die werden dekodiert und ueber
 * setWasmEventHandler() an den WASM-Client weitergereicht (siehe
 * GameCanvasComponent). Alles andere (z.B. "ping") wird weiterhin nur
 * geloggt.
 *
 * Umgekehrte Richtung: Kommandos vom WASM-Client gehen ueber sendCommand()
 * hinaus, im selben Format (Base64-kodiert in {"type":"command","data":...}),
 * nur mit "command" statt "event" -- consumers.py, receive() erwartet diese
 * Form fuer eingehende WebSocket-Nachrichten.
 */
@Injectable({ providedIn: 'root' })
export class GameSocketService {
  private readonly auth = inject(AuthService);
  private socket: WebSocket | null = null;
  /** connect() wartet gerade auf einen gueltigen Token (siehe dort). */
  private connecting = false;
  /** disconnect() kam, waehrend connect() noch wartete. */
  private disconnectRequested = false;
  private heartbeatIntervalId: ReturnType<typeof setInterval> | null = null;
  private wasmEventHandler: WasmEventHandler | null = null;
  private connectionStateHandler: ConnectionStateHandler | null = null;
  private playerIdHandler: PlayerIdHandler | null = null;
  /** Die eigene Spieler-Id dieser Verbindung, sobald das Backend sie geschickt hat. */
  private playerId: string | null = null;

  /**
   * Von GameCanvasComponent aufgerufen, sobald das WASM-Modul geladen ist,
   * damit handleMessage() unten die dekodierten Bytes von "event"-
   * Nachrichten dorthin weiterreichen kann. Mit null wieder abmelden
   * (z.B. in ngOnDestroy), damit nach dem Entladen der Komponente nichts
   * mehr an ein totes Modul geschickt wird.
   */
  setWasmEventHandler(handler: WasmEventHandler | null): void {
    this.wasmEventHandler = handler;
  }

  /**
   * Wie setWasmEventHandler(), nur fuer den Verbindungszustand. Meldet den
   * aktuellen Zustand sofort: connect() wird von CurrentGameComponent
   * unabhaengig vom Laden des WASM-Moduls aufgerufen, und die Verbindung kann
   * schon offen sein, wenn sich der Client anmeldet -- ohne diesen ersten
   * Aufruf erfuehre er davon nie.
   */
  /**
   * Wie setConnectionStateHandler(), nur fuer die eigene, oeffentliche
   * Spieler-Id: das Backend schickt sie gleich nach dem Verbindungsaufbau
   * ({"type":"identity"}, radish/backend/api/consumers.py), oft bevor das
   * WASM-Modul geladen ist. Ist sie schon da, wird sie deshalb sofort gemeldet.
   *
   * Nur die oeffentliche Id -- den geheimen Zucchini-Code vor jeder Nachricht
   * setzt das Backend selbst und schickt ihn nie heraus.
   */
  setPlayerIdHandler(handler: PlayerIdHandler | null): void {
    this.playerIdHandler = handler;
    if (handler && this.playerId !== null) {
      handler(this.playerId);
    }
  }

  setConnectionStateHandler(handler: ConnectionStateHandler | null): void {
    this.connectionStateHandler = handler;
    handler?.(this.socket?.readyState === WebSocket.OPEN);
  }

  connect(): void {
    if (this.connecting) {
      // Ein disconnect() dazwischen gilt nicht mehr: es soll wieder verbunden sein.
      this.disconnectRequested = false;
      return;
    }
    if (this.socket) {
      return;
    }

    // Das Backend prueft den Token nur beim Handshake -- er muss dann aber
    // gelten. Nach einem Hintergrund-Tab ist er womoeglich abgelaufen, deshalb
    // getValidAccessToken(), das ihn vorher erneuert.
    this.connecting = true;
    this.auth
      .getValidAccessToken()
      .then((token) => {
        this.connecting = false;
        if (this.disconnectRequested) {
          this.disconnectRequested = false;
          return;
        }
        this.open(token);
      });
  }

  private open(token: string | null): void {
    if (!token) {
      console.warn('GameSocketService: kein Access-Token vorhanden, WebSocket wird nicht geoeffnet.');
      return;
    }

    // Gleiche Origin wie die App selbst (siehe environment.ts,
    // proxy.conf.json fuer ng serve) -- deshalb aus window.location
    // zusammengesetzt statt aus einer festen Konfiguration.
    const wsProtocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
    const url = `${wsProtocol}://${window.location.host}/ws/echo/?token=${encodeURIComponent(token)}`;

    const socket = new WebSocket(url);
    socket.onopen = () => {
      console.log('GameSocketService: Verbindung geoeffnet.');
      this.startHeartbeat();
      this.connectionStateHandler?.(true);
    };
    socket.onmessage = (event) => this.handleMessage(event.data);
    socket.onerror = (event) => console.error('GameSocketService: Fehler:', event);
    socket.onclose = (event) => {
      console.log(`GameSocketService: Verbindung geschlossen (Code ${event.code}).`);
      this.stopHeartbeat();
      this.socket = null;
      this.playerId = null;
      this.connectionStateHandler?.(false);
    };

    this.socket = socket;
  }

  disconnect(): void {
    // Kommt der Abbau, waehrend connect() noch auf den Token wartet, wird die
    // Verbindung danach gar nicht erst geoeffnet.
    if (this.connecting) {
      this.disconnectRequested = true;
    }
    this.stopHeartbeat();
    this.socket?.close();
    this.socket = null;
  }

  /**
   * Schickt Rohdaten vom WASM-Client als "command"-Nachricht ans Backend
   * (siehe consumers.py, receive()) -- Base64-kodiert in
   * {"type":"command","data":...}, gleiches Format wie eingehende "event"-
   * Nachrichten (siehe handleMessage() unten), nur in die andere Richtung.
   * Wird von GameCanvasComponent aufgerufen (siehe dort,
   * wasmModule.sendToChannel).
   */
  sendCommand(bytes: Uint8Array): void {
    if (this.socket?.readyState !== WebSocket.OPEN) {
      console.warn('GameSocketService: Kommando kann nicht gesendet werden, Verbindung ist nicht offen.');
      return;
    }
    this.socket.send(JSON.stringify({ type: 'command', data: bytesToBase64(bytes) }));
  }

  /**
   * Gleiches Nachrichtenformat wie der Server-Ping (siehe consumers.py,
   * _ping_loop) -- EchoConsumer.receive() erkennt "type":"ping" daran und
   * reicht es nicht an den Spielserver weiter (siehe dort, _is_heartbeat()).
   */
  private startHeartbeat(): void {
    this.stopHeartbeat();
    this.heartbeatIntervalId = setInterval(() => {
      if (this.socket?.readyState !== WebSocket.OPEN) {
        return;
      }
      this.socket.send(JSON.stringify({ type: 'ping', data: { time: new Date().toISOString() } }));
    }, HEARTBEAT_INTERVAL_MS);
  }

  private stopHeartbeat(): void {
    if (this.heartbeatIntervalId === null) {
      return;
    }
    clearInterval(this.heartbeatIntervalId);
    this.heartbeatIntervalId = null;
  }

  /**
   * Parst eine eingehende Nachricht und reicht bei "type":"event" die
   * Base64-dekodierten Rohdaten an den registrierten WASM-Client weiter
   * (siehe setWasmEventHandler()), bei "type":"identity" die eigene
   * Spieler-Id (siehe setPlayerIdHandler()). Alles andere, oder was sich
   * nicht sauber parsen/dekodieren laesst, wird nur geloggt bzw. als
   * Fehler gemeldet, aber nicht weitergereicht.
   */
  private handleMessage(raw: string): void {
    let message: { type?: string; data?: unknown };
    try {
      message = JSON.parse(raw);
    } catch (err) {
      console.error('GameSocketService: Nachricht ist kein gueltiges JSON:', raw, err);
      return;
    }

    if (message.type === 'identity') {
      this.handleIdentity(message.data);
      return;
    }

    if (message.type !== 'event') {
      console.log('GameSocketService: Nachricht vom Backend:', message);
      return;
    }

    if (typeof message.data !== 'string') {
      console.error('GameSocketService: "event"-Nachricht ohne Base64-"data":', message);
      return;
    }

    let bytes: Uint8Array;
    try {
      bytes = Uint8Array.from(atob(message.data), (c) => c.charCodeAt(0));
    } catch (err) {
      console.error('GameSocketService: "data" ist kein gueltiges Base64:', message, err);
      return;
    }

    if (!this.wasmEventHandler) {
      console.warn('GameSocketService: "event" empfangen, aber kein WASM-Client registriert.');
      return;
    }
    this.wasmEventHandler(bytes);
  }

  private handleIdentity(data: unknown): void {
    const playerId = (data as { player_id?: unknown } | undefined)?.player_id;
    if (typeof playerId !== 'string') {
      console.error('GameSocketService: "identity"-Nachricht ohne "player_id":', data);
      return;
    }
    this.playerId = playerId;
    this.playerIdHandler?.(playerId);
  }
}

/** Gegenstueck zum Dekodieren in handleMessage() oben, nur fuer den Versand. */
function bytesToBase64(bytes: Uint8Array): string {
  let binary = '';
  for (const byte of bytes) {
    binary += String.fromCharCode(byte);
  }
  return btoa(binary);
}
