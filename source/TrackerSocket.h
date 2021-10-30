
#include <string>
#include <vector>
#include <cstdint>
#include <thread>
#include <mutex>

#include "gui.h"
#include "map.h"
#include "items.h"
#include "editor_tabs.h"
#include "editor.h"

class SocketConnection;
class SocketMessage;
class TrackerSocket;

class SocketMessage {
public:
	SocketMessage();
	~SocketMessage();

	void Reset();

	uint8_t ReadByte();
	uint8_t ReadU8();
	uint16_t ReadU16();
	uint32_t ReadU32();
	std::string ReadString();
	Position ReadPosition();

	std::vector<uint8_t> buffer;
protected:
	union {
		uint32_t sent;
		uint32_t read;
	};

	friend class SocketConnection;
};

class TrackerNetSocket {
public:
	TrackerNetSocket() {}
	virtual ~TrackerNetSocket() {}

	void FreeMessage(SocketMessage* msg);
	SocketMessage* AllocMessage();

protected:
	std::vector<SocketMessage*> message_pool;
};

typedef void (TrackerSocket::*LiveServerPacketParser)(SocketConnection* , SocketMessage* );

class SocketConnection {
public:
	SocketConnection(TrackerNetSocket* nsocket, wxSocketBase* socket);
	~SocketConnection();

	void Close();

	SocketMessage* Receive();

	LiveServerPacketParser parser;
protected:
	SocketMessage* receiving_message;
	wxSocketBase* socket;
	TrackerNetSocket* nsocket;
	std::deque<SocketMessage*> waiting_messages;
};

class TrackerSocket : public wxEvtHandler, public TrackerNetSocket
{
	~TrackerSocket();
public:
	TrackerSocket(Editor* editor);

	bool Connect();
	void Close();
	void Log(wxString message) {};

	void HandleEvent(wxSocketEvent& evt);

	void OnParseLoginPackets(SocketConnection* connection, SocketMessage* nmsg);

	void RegisterMap(BaseMap* extMap) {
		map = extMap;
	};

protected:
	void OnParsePacket(SocketMessage* nmsg);

	void OnReceiveFullMap(SocketMessage* nmsg);
	void OnReceiveCreateOnMap(SocketMessage* nmsg);
	void OnReceiveDeleteOnMap(SocketMessage* nmsg);
	void OnReceiveChangeOnMap(SocketMessage* nmsg);

	Editor* editor;
	wxSocketClient* socket = nullptr;
	wxSocketServer* serv = nullptr;
	SocketConnection* connection = nullptr;
	bool isOnline = false;

	BaseMap* map = nullptr;
	wxEvtHandler* event_dump;
};
