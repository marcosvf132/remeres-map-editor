//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "TrackerSocket.h"
#include <iostream>


#define TRACKERPORT 8119

extern ItemDatabase g_items;

TrackerSocket::TrackerSocket(Editor* editor) :
	editor(editor)
{
}

TrackerSocket::~TrackerSocket()
{
}

bool TrackerSocket::Connect()
{
	if (!editor || !map)
		return false;

	wxIPV4address ipaddr;

	ipaddr.Hostname(wxT("127.0.0.1"));
	ipaddr.Service(TRACKERPORT);

	serv = newd wxSocketServer(ipaddr, wxSOCKET_NOWAIT);
	bool ok = serv->IsOk();
	if(ok) {
		serv->SetEventHandler(*this, wxID_ANY);
		serv->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_CONNECTION_FLAG);
		serv->Notify(true);
		wxEvtHandler::Connect(wxID_ANY, wxEVT_SOCKET, wxSocketEventHandler(TrackerSocket::HandleEvent));
	}
	return ok;
}

void TrackerSocket::Close()
{
	delete connection;
	connection = NULL;
	if (socket) {
		socket->Destroy();
		socket = NULL;
	}

	delete this;
}

void TrackerSocket::HandleEvent(wxSocketEvent& evt)
{
	try {
		SocketConnection* connection = reinterpret_cast<SocketConnection*>(evt.GetClientData());
		switch (evt.GetSocketEvent()) {
		case wxSOCKET_CONNECTION:
		{
			wxSocketBase* client = serv->Accept(false);
			if (!client)
				return;

			SocketConnection* connection = newd SocketConnection(this, client);
			client->SetClientData(connection);

			client->SetEventHandler(*this, wxID_ANY);
			client->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_CONNECTION_FLAG);
			client->Notify(true);
		} break;
		case wxSOCKET_INPUT:
		{
			SocketMessage* nmsg = connection->Receive();
			if (nmsg) {
				OnParsePacket(nmsg);
				FreeMessage(nmsg);
			}
		} break;
		}
	}
	catch (std::runtime_error& err) {
		g_gui.PopupDialog("[Tracker event handler]", err.what(), wxOK);
	}
}

void TrackerSocket::OnParsePacket(SocketMessage* nmsg)
{
	switch (nmsg->ReadByte()) {
		case 0x03: { // TrackerMessageHeader.FullMap
			OnReceiveFullMap(nmsg);
		} break;
		case 0x04: { // TrackerMessageHeader.CreateOnMap
			OnReceiveCreateOnMap(nmsg);
		} break;
		case 0x05: { // TrackerMessageHeader.DeleteOnMap
			OnReceiveDeleteOnMap(nmsg);
		} break;
		case 0x06: { // TrackerMessageHeader.ChangeOnMap
			OnReceiveChangeOnMap(nmsg);
		} break;
	}
}

void TrackerSocket::OnReceiveFullMap(SocketMessage* nmsg)
{
	Position tilePos = Position(1000, 1000, 7);
	try {
		uint16_t tilesCount = nmsg->ReadU16();
		for (uint16_t it_t = 1; it_t <= tilesCount; it_t++) {
			tilePos = nmsg->ReadPosition();
			Tile* tile = map->getTile(tilePos);
			if (!tile)
				tile = map->createTile(tilePos.x, tilePos.y, tilePos.z);

			if (!tile)
				return;

			uint8_t tileSize = nmsg->ReadByte();
			for (uint8_t it = 1; it <= tileSize; it++) {
				bool isCreature = nmsg->ReadByte() == 0x01;
				if (isCreature) {
					// TO-DO
					//nmsg->ReadString();
				} else {
					uint16_t clientID = nmsg->ReadU16();
					uint16_t subType = nmsg->ReadU16();

					if (tile->getItem(clientID, subType, -1) != nullptr)
						continue;

					Item* item = Item::Create(clientID, subType);
					if (!item)
						continue;

					if (item->isSplash())
						continue;

					item->setSubtype(subType);

					if (item->isGroundTile())
						tile->ground = item;
					else if (item->isWall())
						tile->addWallItem(item);
					else if (item->isBorder())
						tile->addBorderItem(item);
					else
						tile->addItem(item);
				}
			}
		}
	} catch (std::runtime_error& err) {
		g_gui.PopupDialog("[Tracker full map]", err.what(), wxOK);
		return;
	}
	g_gui.SetScreenCenterPosition(tilePos);
}

void TrackerSocket::OnReceiveCreateOnMap(SocketMessage* nmsg)
{
	try {
		Position tilePos = nmsg->ReadPosition();
		Tile* tile = map->getTile(tilePos);
		if (!tile)
			tile = map->createTile(tilePos.x, tilePos.y, tilePos.z);

		if (!tile)
			return;

		// I'm ignoring it for now
		uint8_t stackPos = nmsg->ReadByte();

		bool isCreature = nmsg->ReadByte() == 0x01;
		if (isCreature) {
			// TO-DO
			//nmsg->ReadString();
		} else {

			uint16_t clientID = nmsg->ReadU16();
			uint16_t subType = nmsg->ReadU16();

			Item* item = Item::Create(clientID, subType);
			if (!item)
				return;

			if (item->isSplash())
				return;

			item->setSubtype(subType);

			tile->insertItem(stackPos, item);
		}

	} catch (std::runtime_error& err) {
		g_gui.PopupDialog("[Tracker create on map]", err.what(), wxOK);
	}
}

void TrackerSocket::OnReceiveDeleteOnMap(SocketMessage* nmsg)
{
	try {
		Position tilePos = nmsg->ReadPosition();
		Tile* tile = map->getTile(tilePos);
		if (!tile)
			tile = map->createTile(tilePos.x, tilePos.y, tilePos.z);

		if (!tile)
			return;

		uint8_t stackPos = nmsg->ReadByte();

		tile->removeItem(0, 0, static_cast<int8_t>(stackPos));
	} catch (std::runtime_error& err) {
		g_gui.PopupDialog("[Tracker delete on map]", err.what(), wxOK);
	}
}

void TrackerSocket::OnReceiveChangeOnMap(SocketMessage* nmsg)
{
	try {
		Position tilePos = nmsg->ReadPosition();
		Tile* tile = map->getTile(tilePos);
		if (!tile)
			tile = map->createTile(tilePos.x, tilePos.y, tilePos.z);

		if (!tile)
			return;

		uint8_t stackPos = nmsg->ReadByte();

		Item* item = tile->getItem(0, 0, static_cast<int8_t>(stackPos));
		if (item) {
			uint16_t clientID = nmsg->ReadU16();
			uint16_t subType = nmsg->ReadU16();

			Item* newItem = Item::Create(clientID, subType);
			if (!newItem)
				return;

			tile->changeItem(0, 0, stackPos, newItem);
		}
	} catch (std::runtime_error& err) {
		g_gui.PopupDialog("[Tracker change on map]", err.what(), wxOK);
	}
}

void TrackerNetSocket::FreeMessage(SocketMessage* msg) {
	msg->Reset();
	message_pool.push_back(msg);
}

SocketConnection::SocketConnection(TrackerNetSocket* nsocket, wxSocketBase* socket) :
	socket(socket),
	nsocket(nsocket),
	receiving_message(NULL)
{
}

SocketConnection::~SocketConnection()
{
	while (waiting_messages.size() > 0) {
		delete waiting_messages.back();
		waiting_messages.pop_back();
	}
}

void SocketConnection::Close()
{
	if (socket) {
		socket->Destroy();
		socket = NULL;
	}
}

SocketMessage* SocketConnection::Receive()
{
	if (!receiving_message) {
		uint32_t sz;
		socket->Peek(&sz, 4);
		if (socket->LastCount() < 4)
			return NULL;
		
		if (socket->Error())
			return NULL;

		if (sz > 256 * 1024)
			return NULL;

		socket->Read(&sz, 4);

		receiving_message = nsocket->AllocMessage();
		receiving_message->buffer.resize(sz);
		receiving_message->read = 0;
	}
	std::vector<uint8_t>& buffer = receiving_message->buffer;
	uint32_t& read = receiving_message->read;
	if (buffer.size()) {
		socket->Read(&buffer[read], buffer.size() - read);

		if (socket->Error()) {

			//lsocketz->Close();
			return NULL;
		}

		read += socket->LastCount();
	}

	if (read == buffer.size()) {
		//		std::cout << "Received message ...\n";
		//		for(uint8_t* c = &buffer.front(); c != &buffer.back(); ++c) {
		//			printf("\t%d\t=%.2x\n", c - &buffer.front(), *c);
		//		}
		SocketMessage* tmp = receiving_message;
		read = 0;
		receiving_message = NULL;
		return tmp;
	}
	// Complete message hasn't been read yet.
	return NULL;
}

SocketMessage* TrackerNetSocket::AllocMessage() {
	if (message_pool.size()) {
		SocketMessage* msg = message_pool.back();
		message_pool.pop_back();
		return msg;
	}
	return newd SocketMessage();
}

void SocketMessage::Reset() {
	buffer.resize(4);
	sent = ~0;
}

SocketMessage::SocketMessage() {
	Reset();
}

SocketMessage::~SocketMessage() {
}

uint8_t SocketMessage::ReadU8() {
	if (read + 1 > buffer.size())
		throw std::runtime_error("Read beyond end of network buffer!");
	return buffer[read++];
}

uint8_t SocketMessage::ReadByte() {
	if (read + 1 > buffer.size())
		throw std::runtime_error("Read beyond end of network buffer!");
	return buffer[read++];
}

uint16_t SocketMessage::ReadU16() {
	if (read + 2 > buffer.size())
		throw std::runtime_error("Read beyond end of network buffer!");
	read += 2;
	return *reinterpret_cast<uint16_t*>(&buffer[read - 2]);
}

uint32_t SocketMessage::ReadU32() {
	if (read + 4 > buffer.size())
		throw std::runtime_error("Read beyond end of network buffer!");
	read += 4;
	return *reinterpret_cast<uint32_t*>(&buffer[read - 4]);
}

std::string SocketMessage::ReadString() {
	uint16_t sz = ReadU16();
	if (sz == 0)
		return "";
	if (read + sz > buffer.size())
		throw std::runtime_error("Read beyond end of network buffer!");
	std::string str((const char*)&buffer[read], sz);
	read += sz;
	return str;
}

Position SocketMessage::ReadPosition() {
	Position pos;
	pos.x = ReadU16();
	pos.y = ReadU16();
	pos.z = ReadU8();
	return pos;
}
