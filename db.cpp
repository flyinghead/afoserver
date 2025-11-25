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
#include "db.h"
#include "log.h"
#include <sqlite3.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <algorithm>

static sqlite3 *db;
static std::string dbPath;

static bool openDatabase()
{
	if (db != nullptr)
		return true;
	if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
		ERROR_LOG("Can't open database %s: %s", dbPath.c_str(), sqlite3_errmsg(db));
		return false;
	}
	sqlite3_busy_timeout(db, 1000);
	return true;
}

void closeDatabase()
{
	if (db != nullptr) {
		sqlite3_close(db);
		db = nullptr;
	}
}

void setDatabasePath(const std::string& databasePath)
{
	::dbPath = databasePath;
	if (!openDatabase())
		exit(1);
	closeDatabase();
}

static void throwSqlError(sqlite3 *db)
{
	const char *msg = sqlite3_errmsg(db);
	if (msg != nullptr)
		throw std::runtime_error(msg);
	else
		throw std::runtime_error("SQL Error");
}

class Statement
{
public:
	Statement(sqlite3 *db, const char *sql) : db(db) {
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK)
			throwSqlError(db);
	}

	~Statement() {
		if (stmt != nullptr)
			sqlite3_finalize(stmt);
	}

	void bind(int idx, int v) {
		if (sqlite3_bind_int(stmt, idx, v) != SQLITE_OK)
			throwSqlError(db);
	}
	void bind(int idx, const std::string& s) {
		if (sqlite3_bind_text(stmt, idx, s.c_str(), s.length(), SQLITE_TRANSIENT) != SQLITE_OK)
			throwSqlError(db);
	}
	void bind(int idx, const uint8_t *data, size_t len) {
		if (sqlite3_bind_blob(stmt, idx, data, len, SQLITE_STATIC) != SQLITE_OK)
			throwSqlError(db);
	}

	bool step()
	{
		int rc = sqlite3_step(stmt);
		if (rc == SQLITE_ROW)
			return true;
		if (rc != SQLITE_DONE && rc != SQLITE_OK)
			throwSqlError(db);
		return false;
	}

	int getIntColumn(int idx) {
		return sqlite3_column_int(stmt, idx);
	}
	std::string getStringColumn(int idx) {
		return std::string((const char *)sqlite3_column_text(stmt, idx));
	}
	std::vector<uint8_t> getBlobColumn(int idx)
	{
		std::vector<uint8_t> blob;
		blob.resize(sqlite3_column_bytes(stmt, idx));
		if (!blob.empty())
			memcpy(blob.data(), sqlite3_column_blob(stmt, idx), blob.size());
		return blob;
	}

	int changedRows() {
		return sqlite3_changes(db);
	}

private:
	sqlite3 *db;
	sqlite3_stmt *stmt = nullptr;
};

std::string getTop10Scores()
{
	if (!openDatabase())
		return {};
	try {
		Statement stmt(db, "SELECT SCORE, PLAYER_NAME, ARCADE_NAME, CITY, STATE FROM RANKING ORDER BY SCORE DESC LIMIT 10");
		std::string scores;
		while (stmt.step())
		{
			if (!scores.empty())
				scores += '&';
			scores += stmt.getStringColumn(0) 			// score
					+ ':' + stmt.getStringColumn(1)		// player name
					+ ':' + stmt.getStringColumn(2)		// arcade name
					+ ':' + stmt.getStringColumn(3)		// city
					+ ':' + stmt.getStringColumn(4);	// state
		}
		return scores;
	} catch (const std::runtime_error& e) {
		ERROR_LOG("getUserRecord: %s", e.what());
	}
	return {};
}

void registerNewDcScore(int score, const std::string& player)
{
	registerNewScore(score, player, "Dreamcast", "Dreamcast", "");
}

void registerNewScore(int score, const std::string& player, const std::string& arcade, const std::string& city, const std::string& state)
{
	if (score == 0 || player.empty())
		return;
	if (!openDatabase())
		return;
	try {
		Statement stmt(db, "UPDATE RANKING SET SCORE = MAX(SCORE, ?), DATE = strftime('%s') WHERE PLAYER_NAME = ? AND ARCADE_NAME = ? AND CITY = ? AND STATE = ?");
		stmt.bind(1, score);
		stmt.bind(2, player);
		stmt.bind(3, arcade);
		stmt.bind(4, city);
		stmt.bind(5, state);
		stmt.step();
		if (stmt.changedRows() > 0)
			return;
	} catch (const std::runtime_error& e) {
		ERROR_LOG("registerNewScore UPDATE: %s", e.what());
	}
	try {
		Statement stmt(db, "INSERT INTO RANKING (SCORE, PLAYER_NAME, ARCADE_NAME, CITY, STATE, DATE) VALUES (?, ?, ?, ?, ?, strftime('%s'))");
		stmt.bind(1, score);
		stmt.bind(2, player);
		stmt.bind(3, arcade);
		stmt.bind(4, city);
		stmt.bind(5, state);
		stmt.step();

	} catch (const std::runtime_error& e) {
		ERROR_LOG("registerNewScore INSERT: %s", e.what());
	}
}
