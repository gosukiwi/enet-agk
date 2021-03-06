# ENet bindings for AppGameKit Tier 1
This provides bindings for ENet networking library for AGK. Just copy the
`Commands.txt`, `Windows.dll`, and `Windows64.dll` files to your compiler's
`Plugins` folder. Something like:

    C:\Program Files\Steam\steamapps\common\App Game Kit 2\Tier 1\Compiler\Plugins\EnetAGK
    |_ Commands.txt
    |_ Windows.dll
    |_ Windows64.dll

Note that this only works on Windows. If you want to add other platforms (Linux,
macOS), feel free to make a PR.

# Commands

```
// initializing/deinitializing
Initialize() // returns an integer, 0 on failure, 1 on success
Deinitialize() // returns void, deinitializes ENet and cleans up resources
CreateServer(port, maxConnections) // returns an integer, `hostId` on success. 0 on failure (only supports "localhost" at the moment)
CreateClient() // returns an integer, `hostId` on success. 0 on failure.
DestroyHost(hostId) // returns void, destroys a host and cleans up resources
SetHostCompressWithRangeCoder(hostId) // returns an integer, 1 if succeeded, 0 on failure. Enables ENet's default compressor

// connecting
HostConnect(hostId, host$, port) // returns a `peerId` or 0 if failed
HostConnectAsync(hostId, host$, port) // returns void, creates a thread and tries to connect asynchronously
HostConnectAsyncPoll() // returns a string, whether or not the connection succeeded. The string can be "uninitialized", "started", "failed", or "succeeded"
HostConnectAsyncPeerId() // returns an integer, the peer id of the most recent async connection. If called again, it will return 0.

// main loop
HostService(hostId) // returns the `eventId` or 0 if failed, main polling function called in each frame
HostFlush(hostId) // returns void, flush all the pending packets in the given host, does not trigger events
GetEventType(eventId) // returns a string, the type can be "connect", "disconnect", "receive" or "none"
PeerSend(peerId, message$, mode$) // returns void, send a message to the specified peer, `mode$` can be "reliable", "unsequenced", or "unreliable"
EventPeerSend(eventId, message$ mode$) // returns void, sends a message to the emitter of the event
HostBroadcast(hostId, message$, mode$) // returns void, sends a message to all peers in the host
GetEventData(eventId) // returns a string, the data received
GetEventPeerAddressHost(eventId) // returns a string, the IP of the peer who sent the message
GetEventPeerAddressPort(eventId) // returns an integer, the port used to receive the message from the peer
GetHostAddress(hostId) // returns a string, the ip for this host
GetHostPort(hostId) // returns an integer, the port for this host
```

## Example
Server:
```
// server.agc
#import_plugin EnetAGK as Enet

Enet.Initialize()
host = Enet.CreateServer(1234, 32)
if host = 0 then Log("Failed to create server")
end

do
  event = Enet.HostService(host)
  if event = 0 then continue

  eventType$ = Enet.GetEventType(event)
  if eventType$ = "connect"
    Log("Client connected: " + Enet.GetEventPeerAddressHost(event) + ":" + Str(Enet.GetEventPeerAddressPort(event)))
    ENet.EventPeerSend(event, "Hello there!", "reliable")
  elseif eventType$ = "receive"
    Print("Client sent message: " + Enet.GetEventData(event))
  elseif eventType$ = "disconnect"
    Log("Client disconnected.")
  endif

  Print("[SERVER]")
  Print(Str(ScreenFPS()))
  Sync()
loop
```

Client:
```
// client.agc
#import_plugin EnetAGK as Enet

Enet.Initialize()
host = Enet.CreateClient()
peer = Enet.HostConnect(host, "localhost", 1234)

if peer = 0 then Log("Failed to connect")

do
  event = Enet.HostService(host)
  if event = 0 then continue

  eventType$ = Enet.GetEventType(event)
  if eventType$ = "receive"
    Log("Got message from server: " + Enet.GetEventData(event))
  elseif eventType$ = "disconnect"
    Log("Disconnected from server!")
  endif

  // Note, you don't want to send messages too many times per second unless the
  // server can take it.
  Enet.PeerSend(host, peer, "Hello!", "unreliable")

  Print("[CLIENT]")
  Print(ScreenFPS())
  Sync()
loop
```

__IMPORTANT__ The event ids are reused, they will repeat often and only make
sense when consuming them right away. You can pass them to other functions and
work with them normally, just __do not save a reference to them__, as the event
id will be pointing to another event internally after a few seconds.

# Complete Example
You can see a more complete code example here: https://github.com/gosukiwi/Enet-AGK-Demo/tree/master
