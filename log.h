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
#include <stdint.h>
#include <stddef.h>
#include <string>

inline static void trim(std::string& s) {
	while (s.back() == '\0')
		s.pop_back();
}

namespace Log {
enum LEVEL
{
	ERROR = 0,
	WARNING = 1,
	NOTICE = 2,
	INFO = 3,
	DEBUG = 4,
};
}

void logger(Log::LEVEL level, const char *file, int line, const char *format, ...);

#define ERROR_LOG(...)                      \
	do {                                    \
		logger(Log::ERROR, __FILE__, __LINE__, __VA_ARGS__);    \
	} while (0)

#define WARN_LOG(...)                       \
	do {                                    \
		logger(Log::WARNING, __FILE__, __LINE__, __VA_ARGS__);    \
	} while (0)

#define NOTICE_LOG(...)                     \
	do {                                    \
		logger(Log::NOTICE, __FILE__, __LINE__, __VA_ARGS__);    \
	} while (0)

#define INFO_LOG(...)                       \
	do {                                    \
		logger(Log::INFO, __FILE__, __LINE__, __VA_ARGS__);    \
	} while (0)

#ifndef NDEBUG
#define DEBUG_LOG(...)                      \
	do {                                    \
		logger(Log::DEBUG, __FILE__, __LINE__, __VA_ARGS__);    \
	} while (0)
#else
#define DEBUG_LOG(...)
#endif

void dumpData(const uint8_t *data, size_t len);
