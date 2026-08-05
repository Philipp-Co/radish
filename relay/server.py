import argparse
import asyncio
import json
import logging
import re

import websockets
from aiortc import RTCConfiguration, RTCPeerConnection, RTCSessionDescription

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("relay")

HOST_CANDIDATE_PATTERN = re.compile(
    r"(a=candidate:\S+ \d+ udp \d+ )(\d+\.\d+\.\d+\.\d+)( \d+ typ host)"
)


class UdpBridge(asyncio.DatagramProtocol):
    def __init__(self, channel):
        self.channel = channel

    def datagram_received(self, data, addr):
        if self.channel.readyState == "open":
            self.channel.send(data)


async def wait_for_ice_gathering_complete(pc):
    if pc.iceGatheringState == "complete":
        return
    done = asyncio.Event()

    @pc.on("icegatheringstatechange")
    def on_change():
        if pc.iceGatheringState == "complete":
            done.set()

    await done.wait()


def rewrite_sdp_candidates(sdp, external_ip):
    sdp = HOST_CANDIDATE_PATTERN.sub(r"\g<1>" + external_ip + r"\g<3>", sdp)
    sdp = re.sub(r"c=IN IP4 \d+\.\d+\.\d+\.\d+", "c=IN IP4 " + external_ip, sdp)
    return sdp


def attach_datachannel(pc, channel, udp_host, udp_port):
    loop = asyncio.get_event_loop()
    bridge_ready = loop.create_future()

    async def setup_udp():
        transport, protocol = await loop.create_datagram_endpoint(
            lambda: UdpBridge(channel),
            remote_addr=(udp_host, udp_port),
        )
        bridge_ready.set_result(transport)

    asyncio.ensure_future(setup_udp())

    @channel.on("message")
    def on_message(message):
        async def forward():
            transport = await bridge_ready
            payload = message.encode() if isinstance(message, str) else message
            transport.sendto(payload)

        asyncio.ensure_future(forward())

    @channel.on("close")
    def on_close():
        async def cleanup():
            transport = await bridge_ready
            transport.close()

        asyncio.ensure_future(cleanup())


async def handle_offer(offer_sdp, offer_type, udp_host, udp_port, external_ip):
    # Keine STUN/TURN-Server: wir wollen ausschliesslich Host-Kandidaten, deren IP wir
    # anschliessend selbst auf external_ip umschreiben.
    pc = RTCPeerConnection(configuration=RTCConfiguration(iceServers=[]))

    @pc.on("datachannel")
    def on_datachannel(channel):
        logger.info("DataChannel '%s' geoeffnet, Ziel udp://%s:%d", channel.label, udp_host, udp_port)
        attach_datachannel(pc, channel, udp_host, udp_port)

    @pc.on("connectionstatechange")
    async def on_connectionstatechange():
        logger.info("Connection state: %s", pc.connectionState)
        if pc.connectionState in ("failed", "closed"):
            await pc.close()

    await pc.setRemoteDescription(RTCSessionDescription(sdp=offer_sdp, type=offer_type))
    answer = await pc.createAnswer()
    await pc.setLocalDescription(answer)
    await wait_for_ice_gathering_complete(pc)

    rewritten_sdp = rewrite_sdp_candidates(pc.localDescription.sdp, external_ip)
    return RTCSessionDescription(sdp=rewritten_sdp, type=pc.localDescription.type)


async def handler(websocket, udp_host, udp_port, external_ip):
    async for raw in websocket:
        message = json.loads(raw)
        if message.get("type") != "offer":
            logger.warning("Unerwartete Nachricht ignoriert: %s", message.get("type"))
            continue

        local_description = await handle_offer(message["sdp"], message["type"], udp_host, udp_port, external_ip)
        await websocket.send(json.dumps({
            "type": local_description.type,
            "sdp": local_description.sdp,
        }))


async def main():
    parser = argparse.ArgumentParser(description="WebRTC-DataChannel-zu-UDP Relay-Server")
    parser.add_argument("--listen-host", default="0.0.0.0")
    parser.add_argument("--listen-port", type=int, default=8765)
    parser.add_argument("--udp-host", required=True, help="Ziel-Host, an den DataChannel-Daten per UDP weitergeleitet werden")
    parser.add_argument("--udp-port", type=int, required=True, help="Ziel-Port fuer die UDP-Weiterleitung")
    parser.add_argument(
        "--external-ip", default="127.0.0.1",
        help="Adresse, die anstelle der (vom Browser aus unerreichbaren) Container-internen IP in den ICE-Kandidaten gemeldet wird"
    )
    args = parser.parse_args()

    async def bound_handler(websocket):
        await handler(websocket, args.udp_host, args.udp_port, args.external_ip)

    async with websockets.serve(bound_handler, args.listen_host, args.listen_port):
        logger.info(
            "Relay-Server laeuft auf ws://%s:%d, UDP-Ziel %s:%d, external-ip=%s",
            args.listen_host, args.listen_port, args.udp_host, args.udp_port, args.external_ip,
        )
        await asyncio.Future()


if __name__ == "__main__":
    asyncio.run(main())
