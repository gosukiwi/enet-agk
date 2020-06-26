/*
	EnetAGK Plugin
	It exposes a curated subset of Enet API to Tier 1.
	For more info on Enet: http://enet.bespin.org/
*/

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <stdio.h>
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
	server = enet_host_create(&address, max_clients, 2, 0, 0);
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
	client = enet_host_create(NULL, 1 , 2, 0, 0);

	if (client != NULL) {
		servers[host_count] = client;
		int host_id = host_count + 1;
		host_count++;
		return host_id;
	}

	return 0;
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
	ENetEvent event;
	enet_host_service(host, &event, 0);
	return push_event(event);
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
// End of event-related

DLL_EXPORT void peer_send(int host_id, int peer_id, const char* message)
{
	ENetPeer* peer = get_peer(peer_id);
	ENetHost* host = get_host(host_id);
	ENetPacket* packet = enet_packet_create(message, strlen(message) + 1, ENET_PACKET_FLAG_RELIABLE); // Just reliable packages for now

	// Send the packet to the peer over channel id 0
	enet_peer_send(peer, 0, packet);
	// enet_host_flush(host);
}

DLL_EXPORT void host_broadcast(int host_id, const char* message)
{
	ENetHost* host = get_host(host_id);
	ENetPacket* packet = enet_packet_create(message, strlen(message) + 1, ENET_PACKET_FLAG_RELIABLE); // Just reliable packages for now

	enet_host_broadcast(host, 0, packet);
	// enet_host_flush(host);
}

// Connects to a host and returns a `peer_id`
DLL_EXPORT int host_connect(int host_id, const char* hostname, int port)
{
	if (peer_count >= MAX_PEERS - 1) return 0;

	ENetAddress address;
	ENetEvent event;
	ENetPeer* peer;
	ENetHost* host = get_host(host_id);

	enet_address_set_host(&address, hostname);
	address.port = port;

	/* Initiate the connection, allocating the two channels 0 and 1. */
	peer = enet_host_connect(host, &address, 2, 0);
	if (peer == NULL) {
		return 0;
	}

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