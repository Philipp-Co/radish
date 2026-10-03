#ifndef __RAD_IO_NET_REQUEST_COMMON_H__
#define __RAD_IO_NET_REQUEST_COMMON_H__

///
/// Was sich die Anfragen in io/net_request/ teilen. Nicht unter include/:
/// ausserhalb dieses Verzeichnisses braucht es niemand.
///

///
/// Obergrenze der gepackten NetUserRequest (net_codec.h/net_codec.c). Mehr schickt
/// der Client nicht: Zucchini-Code und Absender setzt das Backend davor. Ein Zug
/// mit allen RAD_NET_PATH_MAX_STEPS Feldern bleibt mit Protobufs
/// Varint-Kodierung deutlich darunter; 128 sind reichlich
/// und ersparen es, die Groesse nachzurechnen. Reicht der Puffer einmal nicht,
/// meldet RAD_NetEncodeMoveRequest das ueber
/// RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL, statt still abzuschneiden.
///
#define RAD_IO_NET_REQUEST_MESSAGE_MAX 128

#endif
