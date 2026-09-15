import { AfterViewInit, Component, ElementRef, Input, OnDestroy, ViewChild, signal } from '@angular/core';

import { environment } from '../../../environments/environment';

/*
 * Emscripten legt beim Laden von client.js eine globale Fabrikfunktion an
 * (Name durch "-s EXPORT_NAME=..." in radish/client/CMakeLists.txt
 * festgelegt). Es gibt dafuer keine offizielle Typdeklaration -- das
 * zurueckgegebene Modul (Module.ccall/._malloc/.HEAPU8/...) kommt aus
 * Emscriptens eigener Laufzeit, deshalb hier bewusst als `any` belassen statt
 * einer selbst ausgedachten, moeglicherweise falschen Typdefinition.
 */
declare global {
  interface Window {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    createZucchiniModule?: (options: Record<string, unknown>) => Promise<any>;
  }
}

type ConnectionStatus = 'connecting' | 'connected' | 'disconnected' | 'error';

/**
 * Das eigentliche Spiel-Canvas samt WASM-Client und WebRTC/Relay-Verbindung
 * -- ausgelagert aus dem, was bis vor Kurzem GameComponent alleine war
 * (siehe deren Docstring-Historie), damit dieselbe Logik an mehreren Stellen
 * eingebettet werden kann, ohne sie zu verlassen bzw. neu zu laden:
 * einerseits weiterhin unter der eigenstaendigen Route "/game/:name" (siehe
 * GameComponent, z.B. nach dem Erstellen/Beitreten eines Spiels), andererseits
 * direkt eingebettet auf "Aktuelles Spiel" (siehe CurrentGameComponent) --
 * dort ausdruecklich ohne Umleitung auf eine andere Seite.
 *
 * gameName ist rein informativ (fuer eine optionale Anzeige durch die
 * einbettende Seite) und fliesst nicht in den Verbindungsaufbau selbst ein --
 * der Relay ist aktuell fest auf eine einzige Spielserver-Instanz verdrahtet,
 * siehe Docstring von connectToRelay() weiter unten.
 */
@Component({
  selector: 'app-game-canvas',
  standalone: true,
  template: `
    <div class="game-canvas">
      <p class="muted">Verbindung: {{ status() }}</p>
      <canvas #canvas width="640" height="420" tabindex="0"></canvas>
    </div>
  `,
  styles: [
    `
      .game-canvas {
        display: flex;
        flex-direction: column;
        gap: 0.5rem;
      }
      canvas {
        border: 1px solid var(--border, #2a2a30);
        display: block;
        background: #000;
        max-width: 100%;
      }
    `,
  ],
})
export class GameCanvasComponent implements AfterViewInit, OnDestroy {
  @ViewChild('canvas') private readonly canvasRef!: ElementRef<HTMLCanvasElement>;

  /** Nur fuer eine moegliche Anzeige durch die einbettende Seite, siehe Klassen-Docstring. */
  @Input() gameName = '';

  readonly status = signal<ConnectionStatus>('connecting');

  private webSocket: WebSocket | null = null;
  private peerConnection: RTCPeerConnection | null = null;
  private scriptEl: HTMLScriptElement | null = null;

  /*
   * Der Emscripten/SDL2-Client haengt seine Tastatur-Handler (keydown/
   * keypress/keyup) global an `window` an -- nicht an das <canvas> --, weil
   * SDL2s HTML5-Backend das absichtlich so macht, damit Tasteneingaben auch
   * ohne zuverlaessigen Canvas-Fokus ankommen (per Browser-DevTools verifiziert:
   * beim Aufruf von createZucchiniModule() registriert client.js u.a.
   * "window:keydown"/"window:keyup"/"window:keypress", nichts davon an
   * `document`). client.js exportiert dafuer kein Aufraeum-Hook
   * (`JSEvents.removeAllEventListeners` ist Teil der Emscripten-Runtime,
   * aber nicht in EXPORTED_RUNTIME_METHODS enthalten -- siehe
   * radish/client/CMakeLists.txt), und ohne eigenes Nachrunterfahren blieben
   * diese `window`-Listener nach dem Verlassen dieser Komponente fuer den
   * Rest der SPA aktiv: jede Taste, die das Spiel waehrend einer laufenden
   * Partie fuer Steuerung nutzt (z.B. WASD/Pfeile), wuerde dann per
   * `e.preventDefault()` global geschluckt -- inklusive in ganz normalen
   * Text-/Passwort-Feldern auf anderen Seiten (siehe games-shell/join).
   * Deshalb werden hier `window.addEventListener` UND (vorsichtshalber,
   * falls eine kuenftige client.js-Version doch an `document` registriert)
   * `document.addEventListener` waehrend des WASM-Ladens abgefangen, um
   * genau die von client.js registrierten Tastatur-Listener zu merken und
   * sie in ngOnDestroy() wieder zu entfernen.
   */
  private readonly capturedKeyListeners: Array<{
    target: Document | Window;
    type: string;
    listener: EventListenerOrEventListenerObject;
    options?: boolean | AddEventListenerOptions;
  }> = [];
  private restoreAddEventListeners: (() => void) | null = null;

  async ngAfterViewInit(): Promise<void> {
    try {
      const wasmModule = await this.loadWasmModule();
      this.connectToRelay(wasmModule);
    } catch (err) {
      console.error('[game-canvas] WASM-Client konnte nicht geladen werden:', err);
      this.status.set('error');
    }
  }

  ngOnDestroy(): void {
    this.webSocket?.close();
    this.peerConnection?.close();
    this.scriptEl?.remove();
    this.releaseGlobalKeyListeners();
  }

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private loadWasmModule(): Promise<any> {
    return new Promise((resolve, reject) => {
      const canvas = this.canvasRef.nativeElement;
      canvas.addEventListener('click', () => canvas.focus());

      const script = document.createElement('script');
      // Von radish/client/CMakeLists.txt (RUNTIME_OUTPUT_DIRECTORY) nach
      // radish/web/ gebaut -- unter environment.wasmBaseUrl eingehaengt,
      // siehe radish/backend/web/ (liefert es unter /wasm/ aus, sowohl im
      // Container als auch bei lokalem `manage.py runserver`) und
      // proxy.conf.json (`ng serve`, reicht /wasm ebenfalls dorthin durch).
      script.src = `${environment.wasmBaseUrl}/client.js`;
      script.onload = () => {
        if (!window.createZucchiniModule) {
          reject(
            new Error('createZucchiniModule steht nach dem Laden von client.js nicht zur Verfuegung.'),
          );
          return;
        }
        window
          .createZucchiniModule({ canvas, print: console.log, printErr: console.error })
          .then(resolve, reject);
      };
      script.onerror = () => reject(new Error(`${script.src} konnte nicht geladen werden.`));
      this.interceptGlobalKeyListeners();
      document.body.appendChild(script);
      this.scriptEl = script;
    });
  }

  /**
   * Faengt window.addEventListener(...)/document.addEventListener(...) fuer
   * "keydown"/"keypress"/"keyup" ab, solange der WASM-Client laedt bzw.
   * laeuft, und merkt sich jeden so registrierten Listener, um ihn in
   * releaseGlobalKeyListeners() gezielt wieder zu entfernen (siehe Kommentar
   * bei capturedKeyListeners oben).
   */
  private interceptGlobalKeyListeners(): void {
    const captured = this.capturedKeyListeners;
    const KEY_EVENT_TYPES = new Set(['keydown', 'keypress', 'keyup']);
    const restoreFns: Array<() => void> = [];

    for (const target of [window, document] as Array<Window | Document>) {
      const original = target.addEventListener.bind(target);
      target.addEventListener = ((
        type: string,
        listener: EventListenerOrEventListenerObject,
        options?: boolean | AddEventListenerOptions,
      ) => {
        if (KEY_EVENT_TYPES.has(type)) {
          captured.push({ target, type, listener, options });
        }
        return original(type, listener, options);
      }) as typeof target.addEventListener;
      restoreFns.push(() => {
        target.addEventListener = original;
      });
    }

    this.restoreAddEventListeners = () => restoreFns.forEach((restore) => restore());
  }

  private releaseGlobalKeyListeners(): void {
    for (const { target, type, listener, options } of this.capturedKeyListeners) {
      target.removeEventListener(type, listener, options);
    }
    this.capturedKeyListeners.length = 0;
    this.restoreAddEventListeners?.();
    this.restoreAddEventListeners = null;
  }

  /**
   * Baut die WebRTC-DataChannel-Verbindung zum Relay auf (siehe
   * radish/relay/server.py) -- inhaltlich unveraendert gegenueber dem
   * bisherigen radish/web/index.html, nur nach TypeScript uebertragen.
   *
   * Bewusst unabhaengig vom konkreten Spiel (gameName): der Relay ist
   * aktuell fest auf eine einzige Spielserver-Instanz verdrahtet
   * (--udp-host/--udp-port beim Start von server.py), und die Matchmaking-API
   * gibt den einem Spiel zugewiesenen Server absichtlich nicht an den Client
   * heraus (siehe radish/backend/api/serializers.py, GameDetailSerializer-
   * Docstring). Eine Verdrahtung "dieses Spiel -> dieser Relay/Server"
   * existiert serverseitig noch nicht und ist ein spaeterer Schritt.
   */
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private connectToRelay(wasmModule: any): void {
    const ws = new WebSocket(environment.relay.signalingUrl);
    this.webSocket = ws;
    let channel: RTCDataChannel | null = null;

    wasmModule.sendToChannel = (bytes: Uint8Array | ArrayBuffer) => {
      if (!channel || channel.readyState !== 'open') {
        return;
      }
      const payload = bytes instanceof ArrayBuffer ? new Uint8Array(bytes) : bytes;
      channel.send(payload);
    };

    const forwardResponseToWasm = (buffer: ArrayBuffer) => {
      const bytes = new Uint8Array(buffer);
      const ptr = wasmModule._malloc(bytes.length);
      wasmModule.HEAPU8.set(bytes, ptr);
      wasmModule.ccall('zuc_on_response', null, ['number', 'number'], [ptr, bytes.length]);
      wasmModule._free(ptr);
    };

    const notifyConnectionState = (state: number) => {
      wasmModule.ccall('zuc_on_connection_state', null, ['number'], [state]);
    };

    ws.onopen = async () => {
      try {
        const pc = new RTCPeerConnection();
        this.peerConnection = pc;
        pc.onconnectionstatechange = () => console.log('[game-canvas] connectionState=' + pc.connectionState);
        pc.oniceconnectionstatechange = () =>
          console.log('[game-canvas] iceConnectionState=' + pc.iceConnectionState);

        channel = pc.createDataChannel('zucchini');
        channel.binaryType = 'arraybuffer';
        channel.onopen = () => {
          this.status.set('connected');
          notifyConnectionState(1);
        };
        channel.onclose = () => {
          this.status.set('disconnected');
          notifyConnectionState(2);
        };
        channel.onerror = (event) => console.error('[game-canvas] channel error:', event);
        channel.onmessage = (event: MessageEvent<ArrayBuffer>) => forwardResponseToWasm(event.data);

        const offer = await pc.createOffer();
        await pc.setLocalDescription(offer);

        await new Promise<void>((resolve) => {
          if (pc.iceGatheringState === 'complete') {
            resolve();
            return;
          }
          pc.onicegatheringstatechange = () => {
            if (pc.iceGatheringState === 'complete') {
              resolve();
            }
          };
        });

        ws.send(JSON.stringify({ type: pc.localDescription!.type, sdp: pc.localDescription!.sdp }));
      } catch (err) {
        console.error('[game-canvas] Verbindungsaufbau fehlgeschlagen:', err);
        this.status.set('error');
      }
    };

    ws.onmessage = async (event) => {
      try {
        const answer = JSON.parse(event.data);
        await this.peerConnection?.setRemoteDescription(answer);
      } catch (err) {
        console.error('[game-canvas] Antwort konnte nicht verarbeitet werden:', err);
        this.status.set('error');
      }
    };

    ws.onerror = () => {
      console.error('[game-canvas] WebSocket-Fehler.');
      this.status.set('error');
    };
    ws.onclose = () => this.status.set('disconnected');
  }
}
