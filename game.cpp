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
#include "game.h"
#include "player.h"

Game::Game(Server& server, asio::io_context& io_context, const std::string& serverIp, uint16_t port)
	: server(server), io_context(io_context), serverIp(serverIp), port(port),
	  socket(io_context, asio::ip::udp::endpoint(asio::ip::address_v4(), port + 1)),
	  pingTimer(io_context)
{
	asio::socket_base::reuse_address option(true);
	socket.set_option(option);
}

void Game::start()
{
	gameAcceptor = GameAcceptor::create(io_context, shared_from_this());
	gameAcceptor->start();
	udpRead();
	// Initial timeout is 10 secs until the game creator connects
	pingTimer.expires_at(asio::chrono::steady_clock::now() + asio::chrono::seconds(10));
	pingTimer.async_wait(std::bind(&Game::onInitialTimeout, shared_from_this(), asio::placeholders::error));
	NOTICE_LOG("Game %s [port %d] started", name.c_str(), port);
}

void Game::setSlots(const std::array<Game::SlotType, 8>& slots) {
	for (size_t i = 0; i < 8; i++)
		this->slots[i].type = slots[i];
}

std::string Game::getHttpDesc(bool attributes) const
{
	std::string s = "Address=" + serverIp
			+ " Port=" + std::to_string(port)
			+ " Response=20"
			+ " GameName=" + name
			+ " GameType=" + std::to_string(type)
			+ " Maps=" + std::to_string(maps)
			+ " Slots=";
	for (auto& slot : slots)
		s += std::to_string(slot.type) + " ";
	s += " Sides=0 0 0 0 1 1 1 1";
	if (attributes)
	{
		std::string attributes = " Attributes=";
		for (int i = 0; i < 8; i++)
		{
			const PlayerSlot& slot = slots[i];
			if (slot.player != nullptr)
			{
				attributes += '0' + std::to_string(i);
				char buf[3];
				for (unsigned j = 0; j < 8; j++)
				{
					uint8_t b = 0;
					if (j < slot.player->getName().length())
						b = slot.player->getName()[j];
					sprintf(buf, "%02x", b);
					attributes += buf;
				}
				for (uint8_t b : slot.player->getExtraData()) {
					sprintf(buf, "%02x", b);
					attributes += buf;
				}
			}
		}
		s += attributes;
	}
	return s;
}

int Game::assignSlot(Player::Ptr player, bool alien)
{
	const size_t start = alien ? 4 : 0;
	for (size_t i = start; i < start + 4; i++)
	{
		PlayerSlot& slot = slots[i];
		if (slot.type == Open || slot.type == Open_CPU)
		{
			slot.openType = slot.type;
			slot.type = Filled;
			slot.player = player;
			slot.lastUdpReceive = asio::chrono::steady_clock::now();
			if (pingSeq == 0)
			{
				// Start ping timer
				pingSeq++;
				pingTimer.expires_at(asio::chrono::steady_clock::now() + asio::chrono::seconds(1));
				pingTimer.async_wait(std::bind(&Game::onPing, shared_from_this(), asio::placeholders::error));
			}
			return i;
		}
	}
	return -1;
}

std::array<uint8_t, 0x82> Game::getPlayerList() const
{
	std::array<uint8_t, 0x82> pkt;
	memset(pkt.data(), 0xfc, sizeof(pkt));
	pkt[0] = 1;
	uint8_t playerCount = 0;
	size_t offset = 2;
	for (auto& slot : slots)
	{
		if (slot.player != nullptr)
		{
			memset(&pkt[offset], 0, 16);
			memcpy(&pkt[offset], &slot.player->getName()[0], slot.player->getName().length());
			memcpy(&pkt[offset + 8], &slot.player->getExtraData()[0], 8);
			playerCount++;
		}
		offset += 16;
	}
	pkt[1] = playerCount;
	return pkt;
}

void Game::sendPlayerList() const
{
	std::array<uint8_t, 0x85> pkt { 0x85, 0x00, 0x01 };
	memcpy(&pkt[3], getPlayerList().data(), 0x82);
	tcpSendToAll(pkt.data(), sizeof(pkt));
}

void Game::udpRead()
{
	socket.async_receive_from(asio::buffer(recvbuf), source,
		[this](const std::error_code& ec, size_t len)
		{
			if (ec)
			{
				if (ec != asio::error::operation_aborted
						&& ec != asio::error::bad_descriptor)
					ERROR_LOG("[port %d] UDP receive_from failed: %s", port, ec.message().c_str());
				return;
			}
			//dump(recvbuf.data(), len);
			Player::Ptr player;
			for (auto& slot : slots)
				if (slot.player != nullptr
						&& slot.player->getUdpEndpoint().address() == source.address()) {
					slot.lastUdpReceive = asio::chrono::steady_clock::now();
					player = slot.player;
					break;
				}
			// TODO alienfnt sends a ping every sec
			//if (player == nullptr)
			//{
			//	for (const auto& spectator : spectators) {
			//		slot.lastUdpReceive = asio::chrono::steady_clock::now();
			//		break;
			//	}
			//}
			if (player != nullptr)
			{
				switch (recvbuf[2])
				{
				case 0x78:
				case 0x03:
					udpSendToAll(recvbuf.data(), len, player);
					break;
				case 0x00:
					// ignore pings?
					break;
				default:
					WARN_LOG("[port %d] UDP packet %02x not handled", port, recvbuf[2]);
					break;
				}
			}
			else {
				WARN_LOG("[port %d] UDP from unknown source: %s:%d", port, source.address().to_string().c_str(), source.port());
			}
			udpRead();
		});

}

void Game::udpSendToAll(const uint8_t *data, size_t len, const Player::Ptr& except)
{
	// TODO wait until we first receive something on UDP before sending?
	std::error_code ec;
	for (auto& slot : slots)
		if (slot.player != nullptr && slot.player != except)
			socket.send_to(asio::buffer(data, len), slot.player->getUdpEndpoint(), 0, ec);
	for (const auto& spectator : spectators)
		socket.send_to(asio::buffer(data, len), spectator->getUdpEndpoint(), 0, ec);
}

void Game::tcpSendToAll(const uint8_t *data, size_t len, const Player::Ptr& except) const
{
	for (auto& slot : slots)
		if (slot.player != nullptr && slot.player != except)
			slot.player->sendTcp(data, len);
	for (const auto& spectator : spectators)
		spectator->sendTcp(data, len);
}

void Game::onInitialTimeout(const std::error_code& ec)
{
	if (ec)
		return;
	NOTICE_LOG("Game %s [port %d] timed out", name.c_str(), port);
	if (gameAcceptor != nullptr) {
		gameAcceptor->stop();
		gameAcceptor = nullptr;
	}
	std::error_code ignored;
	socket.close(ignored);
	server.deleteGame(shared_from_this());
}

void Game::onPing(const std::error_code& ec)
{
	if (ec)
		return;
	uint8_t pkt[] { 0x0a, 0x00, 0x78, (uint8_t)(pingSeq & 0xff), (uint8_t)(pingSeq >> 8), 0x00, 0x00, 0x04, 0x08, 0x08 };
	pingSeq++;
	udpSendToAll(pkt, sizeof(pkt));
	pingTimer.expires_at(asio::chrono::steady_clock::now() + asio::chrono::seconds(1));
	pingTimer.async_wait(std::bind(&Game::onPing, shared_from_this(), asio::placeholders::error));
	// Time out players
	auto now = asio::chrono::steady_clock::now();
	for (auto& slot : slots)
		if (slot.player != nullptr && slot.lastUdpReceive + asio::chrono::seconds(30) <= now) {
			INFO_LOG("[port %d] Player %s has timed out", port, slot.player->getName().c_str());
			disconnect(slot.player);
		}
}

void Game::disconnect(Player::Ptr player)
{
	bool empty = true;
	for (auto& slot : slots)
		if (slot.player == player)
		{
			INFO_LOG("[port %d] Player %s left game %s", port, slot.player->getName().c_str(), name.c_str());
			slot.type = slot.openType;
			slot.player->resetSlotNum();
			slot.player->disconnect();
			slot.player = nullptr;
		}
		else if (slot.player != nullptr) {
			empty = false;
		}
	if (!empty) {
		sendPlayerList();
		return;
	}
	NOTICE_LOG("Game %s [port %d] terminated", name.c_str(), port);
	if (gameAcceptor != nullptr) {
		gameAcceptor->stop();
		gameAcceptor = nullptr;
	}
	std::error_code ec;
	socket.close(ec);
	server.deleteGame(shared_from_this());
}

void Game::addSpectator(Player::Ptr player) {
	spectators.push_back(player);
}

void Game::removeSpectator(Player::Ptr player) {
	auto it = std::remove(spectators.begin(), spectators.end(), player);
	spectators.erase(it, spectators.end());
}

void GameAcceptor::start()
{
	GameConnection::Ptr newConnection = GameConnection::create(io_context);

	acceptor.async_accept(newConnection->getSocket(),
			std::bind(&GameAcceptor::handleAccept, shared_from_this(), newConnection, asio::placeholders::error));
}

void GameAcceptor::handleAccept(GameConnection::Ptr newConnection, const std::error_code& error)
{
	if (!acceptor.is_open())
	  return;

	if (!error)
	{
		Player::Ptr player = Player::create(newConnection, game);
		INFO_LOG("[port %d] New connection from %s", game->getIpPort(), newConnection->getSocket().remote_endpoint().address().to_string().c_str());
		newConnection->setPlayer(player);
		newConnection->start();
	}
	start();
}
