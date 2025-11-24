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
#include "http.h"
#include "game.h"
#include "tomcrypt.h"

class CgiHandler
{
public:
	virtual ~CgiHandler() = default;
	virtual void handleRequest(const Request& request, Reply& reply) {
		WARN_LOG("CGI not found: %s [%s]", request.uri.c_str(), request.content.c_str());
		reply = Reply::stockReply(Reply::not_found);
	}
};

static std::vector<uint8_t> hexStringToBytes(const std::string &s)
{
	std::vector<uint8_t> v;
	v.reserve(s.length() / 2);
	for (std::size_t pos = 0; pos < s.length() - 1; pos += 2)
	{
		int n;
		if (sscanf(&s[pos], "%02x", &n) != 1) {
			ERROR_LOG("Invalid hex string %s", s.c_str());
			v.clear();
			break;
		}
		v.push_back(n);
	}
	return v;
}

static const unsigned char NaomiKey[] = { 0x01, 0xD3, 0xB4, 0x90, 0xAB, 0x32, 0x2D, 0xC7 };
static const unsigned char DreamcastKey[] = { 0xd4, 0x61, 0xdb, 0x19, 0x4a, 0x30, 0x17, 0xbc };

static std::string decrypt(const std::string& hex, const unsigned char *key)
{
	std::vector<uint8_t> ciphered = hexStringToBytes(hex);
	symmetric_key skey;
	rc5_setup(key, sizeof(DreamcastKey), 0, &skey);
	std::vector<uint8_t> plain;
	plain.resize(ciphered.size());
	for (size_t i = 0; i < ciphered.size(); i += 8)
		rc5_ecb_decrypt(ciphered.data() + i, plain.data() + i, &skey);
	rc5_done(&skey);
	return std::string((char *)&plain[0], (char *)&plain[plain.size()]);
}

static std::string descramble(const std::string& cs)
{
	std::vector<uint8_t> data = hexStringToBytes(cs);
	std::string ps;
	ps.reserve(data.size());
	for (uint8_t c : data)
		ps.push_back((char)~((c >> 5) | (c << 3)));
	return ps;
}

class HighScoreHandler : public CgiHandler
{
public:
	void handleRequest(const Request& request, Reply& reply) override
	{
		if (request.content.substr(0, 10) == "request=1 ")
		{
			// TODO Naomi: Register new high score
			std::string s = request.content.substr(10);
			std::string plain = decrypt(s, NaomiKey);
			return;
		}
		if (request.content == "request=2")
		{
			// TODO Naomi: Return top 10 players
			reply.setContent("***100000:FLY:ARCADIA:PARIS:TX&&&");
			return;
		}
		if (request.content.substr(0, 10) == "request=3 ")
		{
			// TODO DC: Return dc high scores
			std::string s = request.content.substr(10);
			//std::string plain = decrypt(s, DreamcastKey);
			//printf("high score list: unciphered=%s\n", plain.c_str());
			reply.setContent("***951000:George:Dreamcast:Dreamcast:&934000:George:Dreamcast:Dreamcast:&660000:RPG:Dreamcast:Dreamcast:"
					"&588600:JJ:Dreamcast:Dreamcast:&510000:Johne:Dreamcast:Dreamcast:&492000:Jako:Dreamcast:Dreamcast:"
					"&412000:Mr. Vid:Dreamcast:Dreamcast:&399600:Sonic94:Dreamcast:Dreamcast:&377000:NiGHTs2:Dreamcast:Dreamcast:"
					"&368000:Topaz95:Dreamcast:Dreamcast:&&&");
			return;
		}
		return CgiHandler::handleRequest(request, reply);
	}

	static HighScoreHandler Instance;
};

HighScoreHandler HighScoreHandler::Instance;

class ServerImpl : public Server
{
public:
	ServerImpl(asio::io_context& io_context)
		: io_context(io_context), signals(io_context), httpServer(io_context, serverIp, 8080)
	{
		signals.add(SIGINT);
		signals.add(SIGTERM);
#if defined(SIGQUIT)
		signals.add(SIGQUIT);
#endif
		signals.async_wait(
			[this](std::error_code /*ec*/, int /*signo*/)
			{
				this->io_context.stop();
			});

		for (uint16_t port = 9400; port < 9420; port++)
			ports.push_back(port);

		// alienfnt: Server2/NaomiNetwork/CGI/Watch
		//           Server2/NaomiNetwork/CGI/SampleCGI4
		//           Server2/NaomiNetwork/CGI/RankingSys/ranking.cgi
		// afo: AFODC/RankingSys/ranking.cgi
		httpServer.addCgiHandler("Server2/NaomiNetwork/CGI/RankingSys/ranking.cgi",
				[](const Request& request, Reply& reply)
				{
					HighScoreHandler::Instance.handleRequest(request, reply);
				});
		httpServer.addCgiHandler("AFODC/RankingSys/ranking.cgi",
				[](const Request& request, Reply& reply)
				{
					HighScoreHandler::Instance.handleRequest(request, reply);
				});
		httpServer.addCgiHandler("AFODC/CGI/AFODCCGI",
				[this](const Request& request, Reply& reply)
				{
					this->handleHttpRequest(request, reply);
				});
	}

	void deleteGame(Game::Ptr game) override
	{
		for (size_t i = 0; i < games.size(); i++)
			if (game == games[i])
			{
				ports.push_back(game->getIpPort());
				games.erase(games.begin() + i);
				return;
			}
		ERROR_LOG("Server::deleteGame game %s [port %d] not found", game->getName().c_str(), game->getIpPort());
	}

private:
	static std::vector<std::string> splitParams(const std::string& str)
	{
		std::vector<std::string> params;
		for (size_t pos = 0; pos < str.length();)
		{
			size_t end = str.find(' ', pos);
			if (end == std::string::npos)
				end = str.length();
			if (end - pos >= 2)
				params.push_back(str.substr(pos, end - pos));
			pos = end + 1;
		}
		return params;
	}

	void handleHttpRequest(const Request& request, Reply& reply)
	{
		int reqType = -1;
		int gamePort = -1;
		std::string playerName;
		std::string replyContent;
		std::vector<std::string> params = splitParams(request.content);
		for (auto& param : params)
		{
			if (param.substr(0, 4) == "PID=")
			{
				std::string value = descramble(param.substr(4));
				if (value.substr(0, 4) == "c*18")
					DEBUG_LOG("PID=%s", value.substr(5).c_str());
			}
			else if (param.substr(0, 8) == "Request=")
			{
				std::string value = descramble(param.substr(8));
				if (value[0] == 'c' && value[1] == '\0') {
					reqType = value[2];
					DEBUG_LOG("Request=%d", reqType);
				}
				else {
					WARN_LOG("*** Unrecognized request: %s", value.c_str());
				}
			}
			else if (param.substr(0, 6) == "Data2=")
			{
				std::string value = descramble(param.substr(6));
				if (value.substr(0, 7) == "i:s:c:c")
					gamePort = (value[12] & 0xff) + (value[13] & 0xff) * 0x100;
			}
			else if (param.substr(0, 6) == "Data3=" && reqType == 2)
			{
				std::string value = descramble(param.substr(6));
				if (value.substr(0, 17) == "c*8:c:c:c:c:s:c:c" && value[17] == '\0')
				{
					playerName = std::string(&value[18], &value[26]);
					trim(playerName);
					DEBUG_LOG("Player: %s (%x %x %x %x %x %x %x)", playerName.c_str(),
							value[26] & 0xff, value[27] & 0xff, value[28] & 0xff, value[29] & 0xff,
							(value[30] & 0xff) + (value[31] & 0xff) * 256, value[32] & 0xff, value[33] & 0xff);
				}
			}
			else if (param.substr(0, 6) == "Data4=" && reqType == 2)
			{
				std::string value = descramble(param.substr(6));
				if (value.substr(0, 16) == "c*16:i:i:c*8:c*8" && value[16] == '\0')
				{
					std::string gameName = std::string(&value[17], &value[33]);
					trim(gameName);
					unsigned gameType, maps;
					memcpy(&gameType, &value[33], 4);
					memcpy(&maps, &value[37], 4);
					std::array<Game::SlotType, 8> slots;
					memcpy(slots.data(), &value[41], sizeof(slots));
					std::array<uint8_t, 8> sides;
					memcpy(sides.data(), &value[49], sizeof(sides));
					Game::Ptr game = Game::create(*this, io_context, ports.back());
					ports.pop_back();
					game->setName(gameName);
					game->setType(gameType);
					game->setMaps(maps);
					game->setSlots(slots);
					games.push_back(game);
					game->start();
					replyContent += game->getHttpDesc(false);
					DEBUG_LOG("Create game: %s", replyContent.c_str());
					replyContent += "\nCREATED\nGAMEDONE\n";
					break;
				}
			}
		}
		if (reqType == -1) {
			CgiHandler error;
			return error.handleRequest(request, reply);
		}
		if (reqType == 0)
		{
			for (const auto& game : games)
				replyContent += game->getHttpDesc(false) + " GAMEDONE\n";
//			replyContent += "Address=146.185.135.179 Port=9407 Response=20 GameName=War is Hell GameType=3 Maps=63 "
//									"Slots=2 0 255 255 0 0 255 255  Sides=0 0 0 0 1 1 1 1 GAMEDONE\n"
//					"Address=146.185.135.179 Port=9408 Response=20 GameName=Alien Fest GameType=1 Maps=63 "
//									"Slots=2 0 255 255 2 0 255 255  Sides=0 0 0 0 1 1 1 1 GAMEDONE\n";
		}
		else if (reqType == 1)
		{
			for (const auto& game : games)
				if (game->getIpPort() == gamePort) {
					replyContent += game->getHttpDesc(true) + "\nGAMEDONE\n";
					break;
				}
		}
		reply.setContent(replyContent + "END\n");
	}

private:
	asio::io_context& io_context;

	/// The signal_set is used to register for process termination notifications.
	asio::signal_set signals;

	HttpServer httpServer;
	std::vector<Game::Ptr> games;
	std::vector<uint16_t> ports;
};

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <server IP address>\n", argv[0]);
		return 1;
	}
	serverIp = argv[1];
	NOTICE_LOG("Alien Front Online server started");
	try {
		asio::io_context io_context;
		ServerImpl server(io_context);
		io_context.run();
	}
	catch (const std::exception& e) {
		ERROR_LOG("Fatal exception: %s", e.what());
	}
	NOTICE_LOG("Alien Front Online server stopped");
}
