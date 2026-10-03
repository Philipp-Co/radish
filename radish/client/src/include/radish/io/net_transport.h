#ifndef __RAD_IO_NET_TRANSPORT_H__
#define __RAD_IO_NET_TRANSPORT_H__

#include <stddef.h>
#include <stdint.h>

///
/// Schickt gepackte Bytes ueber die einbettende Seite an das Backend
/// (Module.sendToChannel, frontend/.../game-canvas.component.ts). Ist dort kein
/// Kanal gesetzt, gehen die Bytes still verloren.
///
/// Der einzige Weg nach draussen: wer etwas verschickt, ruft das hier und kennt
/// weder EM_JS noch Emscripten.
///
void RAD_IoNetTransmit(const uint8_t *data, size_t length);

#endif
