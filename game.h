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

extern std::string serverIp;

class GameAcceptor;
class Server;
class Player;

class Game : public SharedThis<Game>
{
public:
	enum SlotType : uint8_t {
		Open = 0,
		Open_CPU = 1,
		Filled = 2,
		CPU = 3,
		Balanced = 4,	// Found in game code but unknown usage
		Filled2 = 5,	// Same
		Closed = 255
	};
	enum GameType : uint8_t {
		None = 0,
		Watch = 1,			// Found in game code but unknown usage
		Competition = 2,	// Same
		DeathMatch = 3,
		TeamFortress = 4,
		CaptureTheFlag = 5,
	};

	void start();
	uint16_t getIpPort() const { return port; }

	const std::string& getName() const { return name; }
	void setName(const std::string& name) { this->name = name; }

	GameType getType() const { return type; }
	void setType(GameType type) { this->type = type; }

	unsigned getMaps() const { return maps; }
	void setMaps(unsigned maps) { this->maps = maps; }

	void setSlots(const std::array<SlotType, 8>& slots);

	std::shared_ptr<Player> getPlayer(int slot) const { return slots[slot].player; }

	std::string getHttpDesc(bool attributes) const;

	int assignSlot(std::shared_ptr<Player> player, bool alien);

	void sendPlayerList() const;

	void tcpSendToAll(const uint8_t *data, size_t len, const std::shared_ptr<Player>& except = nullptr) const;

	void disconnect(std::shared_ptr<Player> player);

private:
	Game(Server& server, asio::io_context& io_context, uint16_t port);
	void udpRead();
	void onPing(const std::error_code& ec);
	void udpSendToAll(const uint8_t *data, size_t len, const std::shared_ptr<Player>& except = nullptr);
	std::array<uint8_t, 0x82> getPlayerList() const;

	Server& server;
	asio::io_context& io_context;
	std::string name;
	uint16_t port;
	GameType type {};
	unsigned maps = 0;
	struct PlayerSlot
	{
		SlotType type = Closed;
		SlotType openType = Closed;	// to restore Open or Open/CPU when player leaves
		std::shared_ptr<Player> player;
		asio::chrono::time_point<asio::chrono::steady_clock> lastUdpReceive;
	};
	std::array<PlayerSlot, 8> slots;
	std::shared_ptr<GameAcceptor> gameAcceptor;
	// UDP socket stuff
	asio::ip::udp::socket socket;
	std::array<uint8_t, 1510> recvbuf;
	asio::ip::udp::endpoint source;	// source endpoint when receiving UDP packets
	asio::steady_timer pingTimer;
	uint16_t pingSeq = 1;

	friend super;
};

class GameConnection;

class GameAcceptor : public SharedThis<GameAcceptor>
{
public:
	void start();

	void stop() {
		std::error_code ec;
		acceptor.close(ec);
	}

private:
	GameAcceptor(asio::io_context& io_context, std::shared_ptr<Game> game)
		: io_context(io_context),
		  acceptor(asio::ip::tcp::acceptor(io_context,
				asio::ip::tcp::endpoint(asio::ip::tcp::v4(), game->getIpPort()))),
				game(game)
	{
		asio::socket_base::reuse_address option(true);
		acceptor.set_option(option);
	}

	void handleAccept(std::shared_ptr<GameConnection> newConnection, const std::error_code& error);

	asio::io_context& io_context;
	asio::ip::tcp::acceptor acceptor;
	std::shared_ptr<Game> game;

	friend super;
};

class Server
{
public:
	virtual ~Server() = default;
	virtual void deleteGame(Game::Ptr game) = 0;
};
