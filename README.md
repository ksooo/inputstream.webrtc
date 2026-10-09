# WebRTC Inputstream

Kodi inputstream add-on for WebRTC live streams, such as camera feeds. Streams are received from
servers that support the [WebRTC-HTTP Egress Protocol (WHEP)](https://datatracker.ietf.org/doc/draft-ietf-wish-whep/),
e.g. [MediaMTX](https://github.com/bluenviron/mediamtx) or [go2rtc](https://github.com/AlexxIT/go2rtc),
and from the cameras of [Home Assistant](https://www.home-assistant.io).

WebRTC is handled by [libdatachannel](https://github.com/paullouisageneau/libdatachannel); Kodi
decodes the received audio and video.

## Usage

The streams are ordinary http or https URLs, so Kodi has to be told to use the add-on with the
`inputstream` property, e.g. in a `.strm` file or an M3U playlist:

```
#KODIPROP:inputstream=inputstream.webrtc
http://camera-server:8889/garden/whep
```

A plugin sets the same property on its list item:
`listitem.setProperty("inputstream", "inputstream.webrtc")`.

Further properties:

| Property | Description |
|---|---|
| `inputstream.webrtc.signaling` | `whep` (default) or `homeassistant` |
| `inputstream.webrtc.entity_id` | The camera entity, for Home Assistant |
| `inputstream.webrtc.bearer_token` | Token sent in the `Authorization: Bearer` header of the WHEP requests; for Home Assistant a long-lived access token |
| `inputstream.webrtc.ice_servers` | Comma separated STUN/TURN servers, e.g. `stun:host:3478,turn:user:password@host:3478`, in addition to the ones announced by Home Assistant |
| `inputstream.webrtc.audio` | `false` to receive video only, which starts playback with less delay |

Other HTTP headers can be appended to the URL in Kodi's usual way, e.g.
`http://host/whep|Authorization=Basic%20dXNlcjpwYXNz`.

TURN servers are only used over UDP.

### Home Assistant

The URL is the address of Home Assistant, also a Home Assistant Cloud remote URL:

```
#KODIPROP:inputstream=inputstream.webrtc
#KODIPROP:inputstream.webrtc.signaling=homeassistant
#KODIPROP:inputstream.webrtc.entity_id=camera.garden
#KODIPROP:inputstream.webrtc.bearer_token=eyJhbGciOi...
http://homeassistant.local:8123
```

The access token grants everything its user may do, so create it for a separate user without
administrator rights. Home Assistant has to support WebRTC for the camera, as it does through its
built-in go2rtc. For https, the server certificate is verified against the root certificates Kodi
provides, or those of the Linux distribution.

## Build instructions

### Linux / macOS

1. `git clone --branch Piers https://github.com/xbmc/xbmc.git`
2. `git clone https://github.com/ksooo/inputstream.webrtc.git`
3. `cd inputstream.webrtc && mkdir build && cd build`
4. `cmake -DADDONS_TO_BUILD=inputstream.webrtc -DADDON_SRC_PREFIX=../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../../xbmc/build/addons -DPACKAGE_ZIP=1 ../../xbmc/cmake/addons`
5. `make`

### Unit tests

The unit tests are built along with the add-on unless cross compiling. Run them with `ctest` in
the add-on's build directory (`build/inputstream.webrtc-prefix/src/inputstream.webrtc-build`).

## Trademark

The WebRTC logo used in the add-on icon belongs to its owner and is not covered by the GPL license
of this add-on. The icon is based on the logo file of [SVG Logos](https://github.com/gilbarbara/logos).
