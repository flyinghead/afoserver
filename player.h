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
#pragma once
#include "shared_this.h"
#include "asio.h"
#include "log.h"

class Player;

class GameConnection : public SharedThis<GameConnection>
{
public:
	asio::ip::tcp::socket& getSocket() {
		return socket;
	}
	void setPlayer(std::shared_ptr<Player> player) {
		this->player = player;
	}

	void receive();

	void sendPacket(uint8_t opcode, const uint8_t *payload = nullptr, unsigned size = 0);

	void close();

private:
	GameConnection(asio::io_context& io_context)
		: io_context(io_context), socket(io_context)
	{
	}

	void send();
	void onSent(const std::error_code& ec, size_t len);

	using iterator = asio::buffers_iterator<asio::const_buffers_1>;

	std::pair<iterator, bool>
	static packetMatcher(iterator begin, iterator end)
	{
		if (end - begin < 3)
			return std::make_pair(begin, false);
		iterator i = begin;
		uint16_t len = (uint8_t)*i++;
		len |= uint8_t(*i++) << 8;
		if (end - begin < len)
			return std::make_pair(begin, false);
		return std::make_pair(begin + len, true);
	}

	void onReceive(const std::error_code& ec, size_t len);

	asio::io_context& io_context;
	asio::ip::tcp::socket socket;
	DynamicBuffer recvBuffer;
	std::array<uint8_t, 8500> sendBuffer;
	size_t sendIdx = 0;
	bool sending = false;
	std::shared_ptr<Player> player;

	friend super;
};

class Game;

class Player : public SharedThis<Player>
{
public:
	const std::string& getName() const { return name; }
	void setName(const std::string& name) { this->name = name; }

	const std::array<uint8_t, 8>& getExtraData() const { return extraData; }
	void setExtraData(const uint8_t *data) { memcpy(extraData.data(), data, sizeof(extraData)); }

	std::string getIp() const {
		return endpoint.address().to_string();
	}
	const asio::ip::udp::endpoint& getUdpEndpoint() const { return endpoint; }

	int assignSlot(bool alien);
	void resetSlotNum() { slotNum = -1; }

	void receiveTcp(const uint8_t *data, size_t len);
	void sendTcp(const uint8_t *data, size_t len);
	void disconnect();

private:
	Player(GameConnection::Ptr connection, std::shared_ptr<Game> game)
		: endpoint(connection->getSocket().remote_endpoint().address(), 7980),
		  connection(connection), game(game)
	{
	}

	std::string name;
	asio::ip::udp::endpoint endpoint;
	std::shared_ptr<GameConnection> connection;
	std::shared_ptr<Game> game;
	std::array<uint8_t, 8> extraData;	// offset 1: 0=army, 1=alien
	int slotNum = -1;

	friend super;
};
