/*
	Game server for Alien Front Online.
    Copyright (C) 2025  Flyinghead

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "player.h"
#include "game.h"
#include "discord.h"

void GameConnection::receive() {
	asio::async_read_until(socket, recvBuffer, packetMatcher,
			std::bind(&GameConnection::onReceive, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
}

void GameConnection::sendPacket(uint8_t opcode, const uint8_t *payload, unsigned size)
{
	*(uint16_t *)&sendBuffer[sendIdx] = size + 3;
	sendBuffer[sendIdx + 2] = opcode;
	if (size != 0)
		memcpy(&sendBuffer[sendIdx + 3], payload, size);
	sendIdx += size + 3;
	send();
}

void GameConnection::send()
{
	if (sending)
		return;
	sending = true;
	uint16_t packetSize = *(uint16_t *)&sendBuffer[0];
	asio::async_write(socket, asio::buffer(sendBuffer, packetSize),
		std::bind(&GameConnection::onSent, shared_from_this(),
				asio::placeholders::error,
				asio::placeholders::bytes_transferred));
}

void GameConnection::onReceive(const std::error_code& ec, size_t len)
{
	if (ec || len < 3)
	{
		std::string addr;
		if (player)
			addr = player->getIp();
		else
			addr = "?.?.?.?";
		if (ec && ec != asio::error::eof && ec != asio::error::operation_aborted
				&& ec != asio::error::bad_descriptor)
			ERROR_LOG("[%s] onReceive: %s", addr.c_str(), ec.message().c_str());
		else if (len != 0)
			ERROR_LOG("[%s] onReceive: small packet: %zd", addr.c_str(), len);
		if (player)
			player->disconnect();
		return;
	}
	// Grab data and process if correct.
	player->receiveTcp(recvBuffer.bytes(), len);
	recvBuffer.consume(len);
	receive();
}

void GameConnection::onSent(const std::error_code& ec, size_t len)
{
	if (ec)
	{
		if (ec != asio::error::eof && ec != asio::error::bad_descriptor)
			ERROR_LOG("onSent: %s", ec.message().c_str());
		if (player)
			player->disconnect();
		return;
	}
	sending = false;
	assert(len <= sendIdx);
	sendIdx -= len;
	if (sendIdx != 0) {
		memmove(&sendBuffer[0], &sendBuffer[len], sendIdx);
		send();
	}
}

void GameConnection::close()
{
	std::error_code ec;
	socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
	player = nullptr;
}

void Player::receiveTcp(const uint8_t *data, size_t len)
{
	switch (data[2])
	{
	case 0: // Login
		{
			if (len < 43) {
				WARN_LOG("TCP packet 0 is too short: %zd", len);
				break;
			}
			// port is assumed to be 7980 (offset 5)
			std::string userName(&data[27], &data[27 + 8]);
			trim(userName);
			name = userName;
			setExtraData(&data[27 + 8]);
			bool alien = (bool)data[36];
			assignSlot(alien);
			uint8_t data[] { 1, (uint8_t)slotNum, 0 };
			connection->sendPacket(0, data, sizeof(data));

			game->sendPlayerList();
			int armySlots = 0, alienSlots = 0;
			std::vector<std::string> players;
			for (int i = 0; i < 8; i++)
			{
				if (game->getSlotType(i) == Game::Open || game->getSlotType(i) == Game::Open_CPU)
				{
					if (i >= 4)
						alienSlots++;
					else
						armySlots++;
				}
				else if (game->getPlayer(i) != nullptr) {
					players.push_back(game->getPlayer(i)->getName());
				}
			}
			if (players.size() == 1)
				discordGameCreated(game->getType(), game->getName(), name, armySlots, alienSlots);
			else
				discordGameJoined(game->getType(), game->getName(), name, players, armySlots, alienSlots);

			break;
		}
	case 1: // Update extra data?
		{
			if (len < 20) {
				WARN_LOG("TCP packet 1 is too short: %zd", len);
			}
			else
			{
				setExtraData(&data[12]);
				// broadcast as 14 00 02 ...
				std::array<uint8_t, 20> out;
				memcpy(out.data(), data, sizeof(out));
				out[2] = 2;
				game->tcpSendToAll(out.data(), sizeof(out), shared_from_this());
			}
			break;
		}
	case 7: // Player list ack'ed ?
		DEBUG_LOG("Player list ack'ed");
		break;
	case 0x78: // ???
		DEBUG_LOG("Packet 78");
		connection->sendPacket(0x78, &data[3], len - 3);
		// broadcast to other players
		game->tcpSendToAll(data, len, shared_from_this());
		break;
	default:
		WARN_LOG("Unhandled game packet: %02x", data[2]);
		break;
	}
}

void Player::sendTcp(const uint8_t *data, size_t len) {
	connection->sendPacket(data[2], &data[3], len - 3);
}

int Player::assignSlot(bool alien)
{
	slotNum = game->assignSlot(shared_from_this(), alien);
	DEBUG_LOG("Player %s assigned slot %d", name.c_str(), slotNum);
	return slotNum;
}

void Player::disconnect()
{
	if (connection != nullptr) {
		connection->close();
		connection = nullptr;
	}
	if (game != nullptr)
	{
		// avoid endless recursivity
		auto lgame = game;
		game = nullptr;
		lgame->disconnect(shared_from_this());
	}
}
