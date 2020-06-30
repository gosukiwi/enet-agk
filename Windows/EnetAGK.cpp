/*
	EnetAGK Plugin
	It exposes a curated subset of Enet API to Tier 1.
	For more info on Enet: http://enet.bespin.org/
*/

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <stdio.h>
#include <thread>
#include "../AGKLibraryCommands.h"
#include "enet/enet.h"

// State
// =====
#define MAX_HOSTS	32
#define MAX_EVENTS	128
#define MAX_PEERS	1024

ENetHost* servers[MAX_HOSTS];
int host_count = 0;

ENetEvent events[MAX_EVENTS];
int event_count = 0;

ENetPeer* peers[MAX_PEERS];
int peer_count = 0;

// Utility functions
// =================
ENetHost* get_host(int host_id)
{
	int index = host_id - 1;
	if (index < 0 || index > MAX_HOSTS - 1) return NULL;

	return servers[index];
}

// This manages a fixed array of events. If it overflows, it starts from 0 again.
// These are meant to be consumed immediately, then thrown away.
// It returns an `event_id`, which is basically `index + 1`.
int push_event(ENetEvent event)
{
	events[event_count] = event;
	int event_id = event_count + 1;
	event_count = (event_count + 1) % MAX_EVENTS;
	return event_id;
}

ENetPeer* get_peer(int peer_id)
{
	int index = peer_id - 1;
	if (index < 0 || index > MAX_PEERS - 1) return NULL;

	return peers[index];
}

char* create_agk_string(const char* str)
{
	int len = (int)strlen(str) + 1;
	char* str2 = agk::CreateString(len);
	strcpy_s(str2, len, str);
	return str2;
}

ENetPacket* create_enet_packet(const char* message, const char* flag_str)
{
	enet_uint32 flags = ENET_PACKET_FLAG_RELIABLE;
	if (strcmp("unsequenced", flag_str) == 0) {
		flags = ENET_PACKET_FLAG_UNSEQUENCED;
	} else if (strcmp("reliable", flag_str) == 0) {
		flags = ENET_PACKET_FLAG_RELIABLE;
	} else if (strcmp("unreliable", flag_str) == 0) {
		flags = 0;
	}

	return enet_packet_create(message, strlen(message) + 1, flags);
}

enum AsyncConnectStatus {
	ASYNC_CONNECT_UNINITIALIZED = 0,
	ASYNC_CONNECT_STARTED		= 1,
	ASYNC_CONNECT_FAILED		= 2,
	ASYNC_CONNECT_SUCCEEDED		= 3
};
AsyncConnectStatus async_connect_status = ASYNC_CONNECT_UNINITIALIZED;
int async_connect_peer_id = 0;
void helper_host_connect_async(int host_id, const char* hostname, int port)
{
	async_connect_status = ASYNC_CONNECT_STARTED;

	if (peer_count >= MAX_PEERS - 1) {
		async_connect_status = ASYNC_CONNECT_FAILED;
		return;
	}

	ENetAddress address;
	ENetEvent event;
	ENetPeer* peer;
	ENetHost* host = get_host(host_id);

	if (host == NULL) {
		async_connect_status = ASYNC_CONNECT_FAILED;
		return;
	}


	enet_address_set_host(&address, hostname);
	address.port = port;

	peer = enet_host_connect(host, &address, 1, 0);
	if (peer == NULL) {
		async_connect_status = ASYNC_CONNECT_FAILED;
		return;
	}

	/* Wait up to 5 seconds for the connection attempt to succeed. */
	if (enet_host_service(host, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
		peers[peer_count] = peer;
		async_connect_peer_id = peer_count + 1;
		peer_count++;
		async_connect_status = ASYNC_CONNECT_SUCCEEDED;
		return;
	}

	/* Either the 5 seconds are up or a disconnect event was */
	/* received. Reset the peer in the event the 5 seconds   */
	/* had run out without any significant event.            */
	enet_peer_reset(peer);
	async_connect_status = ASYNC_CONNECT_FAILED;
}

// Exports
// =======
DLL_EXPORT int initialize()
{
	return enet_initialize();
}

DLL_EXPORT void deinitialize()
{
	enet_deinitialize();
}

// Creates a localhost server on the specified port.
// Returns 0 on failure, and 1+ on success. The returned number represents the `host_id`
DLL_EXPORT int create_server(int port, int max_clients)
{
	if (host_count >= MAX_HOSTS - 1) return 0;

	ENetAddress address;
	ENetHost* server;

	address.host = ENET_HOST_ANY;
	address.port = port;
	server = enet_host_create(&address, max_clients, 1, 0, 0);
	int host_id = 0;

	if (server != NULL) {
		servers[host_count] = server;
		host_id = host_count + 1;
		host_count++;
	}

	return host_id;
}

// Creates a host, returns a `host_id` on success, 0 on failure.
DLL_EXPORT int create_client()
{
	if (host_count >= MAX_HOSTS - 1) return 0;

	ENetHost* client;
	client = enet_host_create(NULL, 1 , 1, 0, 0);

	if (client != NULL) {
		servers[host_count] = client;
		int host_id = host_count + 1;
		host_count++;
		return host_id;
	}

	return 0;
}

// Connects to a host and returns a `peer_id`
DLL_EXPORT int host_connect(int host_id, const char* hostname, int port)
{
	if (peer_count >= MAX_PEERS - 1) return 0;

	ENetAddress address;
	ENetEvent event;
	ENetPeer* peer;
	ENetHost* host = get_host(host_id);

	if (host == NULL) return 0;

	enet_address_set_host(&address, hostname);
	address.port = port;

	peer = enet_host_connect(host, &address, 1, 0);
	if (peer == NULL) return 0;

	/* Wait up to 5 seconds for the connection attempt to succeed. */
	if (enet_host_service(host, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
		peers[peer_count] = peer;
		int peer_id = peer_count + 1;
		peer_count++;
		return peer_id;
	}

	/* Either the 5 seconds are up or a disconnect event was */
	/* received. Reset the peer in the event the 5 seconds   */
	/* had run out without any significant event.            */
	enet_peer_reset(peer);
	return 0;
}

DLL_EXPORT void host_connect_async(int host_id, const char* hostname, int port)
{
	std::thread t1(helper_host_connect_async, host_id, hostname, port);
	t1.detach();
}

DLL_EXPORT char* host_connect_async_poll()
{
	switch (async_connect_status) {
	case ASYNC_CONNECT_UNINITIALIZED:
		return create_agk_string("uninitialized");
	case ASYNC_CONNECT_STARTED:
		return create_agk_string("started");
	case ASYNC_CONNECT_FAILED:
		return create_agk_string("failed");
	case ASYNC_CONNECT_SUCCEEDED:
		async_connect_status = ASYNC_CONNECT_UNINITIALIZED;
		return create_agk_string("succeeded");
	}

	return create_agk_string("failed");
}

DLL_EXPORT int host_connect_async_peer_id()
{
	int peer_id = async_connect_peer_id;
	async_connect_peer_id = 0;
	return peer_id;
}

DLL_EXPORT void destroy_host(int host_id)
{
	ENetHost* host = get_host(host_id);
	if (host != NULL) enet_host_destroy(host);
}

// Main polling function. It returns an `event_id` which AGK can use to
// get info from this particular event. Note that IDs have a small cap and
// will be re-used. They are meant to be used immediately and then thrown away.
DLL_EXPORT int host_service(int host_id)
{
	ENetHost* host = get_host(host_id);
	if (host == NULL) return 0;

	ENetEvent event;
	int result = enet_host_service(host, &event, 0);
	if (result == 0) return 0;

	return push_event(event);
}

DLL_EXPORT void host_flush(int host_id)
{
	ENetHost* host = get_host(host_id);
	if (host == NULL) return;

	enet_host_flush(host);
}

DLL_EXPORT char* get_host_address(int host_id)
{
	ENetHost* host = get_host(host_id);
	if (host == NULL) return create_agk_string("");

	char str[16];
	int ip = host->address.host;
	unsigned char bytes[4];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;   
    sprintf(str, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
	return create_agk_string(str);
}

DLL_EXPORT int get_host_port(int host_id)
{
	ENetHost* host = get_host(host_id);
	if (host == NULL) return 0;

	return host->address.port;
}

DLL_EXPORT int set_host_compress_with_range_coder(int host_id)
{
	ENetHost* host = get_host(host_id);
	if (host == NULL) return 0;
	if (enet_host_compress_with_range_coder(host) == 0) return 1;

	return 0;
}

// Event-related
DLL_EXPORT char* get_event_type(int event_id)
{
	int index = event_id - 1;
	if (index < 0 || index > MAX_EVENTS - 1) return create_agk_string("undefined");

	ENetEvent event = events[index];
	switch (event.type) {
	case ENET_EVENT_TYPE_NONE:
		return create_agk_string("none");
	case ENET_EVENT_TYPE_CONNECT:
		return create_agk_string("connect");
	case ENET_EVENT_TYPE_DISCONNECT:
		return create_agk_string("disconnect");
	case ENET_EVENT_TYPE_RECEIVE:
		return create_agk_string("receive");
	}

	return create_agk_string("undefined");
}

DLL_EXPORT char* get_event_data(int event_id)
{
	int index = event_id - 1;
	if (index < 0 || index > MAX_EVENTS - 1) return create_agk_string("");

	ENetEvent event = events[index];

	// Get the data, cast it to string and return an AGK string
	char* str = (char*)malloc(event.packet->dataLength);
	sprintf(str, "%s", event.packet->data);
	char* agk_string = create_agk_string(str);
	free(str);
	return agk_string;
}

DLL_EXPORT char* get_event_peer_address_host(int event_id)
{
	int index = event_id - 1;
	if (index < 0 || index > MAX_EVENTS - 1) return 0;

	ENetEvent event = events[index];
	char str[16];
	int ip = event.peer->address.host;
	unsigned char bytes[4];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;   
    sprintf(str, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
	return create_agk_string(str);
}

DLL_EXPORT int get_event_peer_address_port(int event_id)
{
	int index = event_id - 1;
	if (index < 0 || index > MAX_EVENTS - 1) return 0;

	ENetEvent event = events[index];
	return event.peer->address.port;
}

DLL_EXPORT void event_peer_send(int event_id, const char* message, const char* flag)
{
	int index = event_id - 1;
	if (index < 0 || index > MAX_EVENTS - 1) return;

	ENetEvent event = events[index];
	ENetPacket* packet = create_enet_packet(message, flag);

	// Send the packet to the peer over channel id 0
	enet_peer_send(event.peer, 0, packet);
}
// End of event-related

DLL_EXPORT void peer_send(int peer_id, const char* message, const char* flag)
{
	ENetPeer* peer = get_peer(peer_id);
	ENetPacket* packet = create_enet_packet(message, flag);

	// Send the packet to the peer over channel id 0
	enet_peer_send(peer, 0, packet);
}

DLL_EXPORT void host_broadcast(int host_id, const char* message, const char* flag)
{
	ENetHost* host = get_host(host_id);
	ENetPacket* packet = create_enet_packet(message, flag);

	enet_host_broadcast(host, 0, packet);
}
