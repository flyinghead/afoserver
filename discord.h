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
#include "game.h"
#include <string>
#include <vector>

void setDiscordWebhook(const std::string& url);
void discordGameCreated(Game::GameType gameType, const std::string& gameName, const std::string& username,
		int armySlots, int alienSlots);
void discordGameJoined(Game::GameType gameType, const std::string& gameName, const std::string& username,
		const std::vector<std::string>& playerList, int armySlots, int alienSlots);
