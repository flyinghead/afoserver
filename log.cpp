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
#include "log.h"
#include <string>
#include <cstdarg>
#include <cstdio>

const char *LevelNames[] = {
	"ERROR",
	"WARNING",
	"NOTICE",
	"INFO",
	"DEBUG",
};

void logger(Log::LEVEL level, const char* file, int line, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	char *temp;
	if (vasprintf(&temp, format, args) < 0)
		throw std::bad_alloc();
	va_end(args);

	time_t now = time(nullptr);
	struct tm tm = *localtime(&now);

	char *msg;
	const int len = asprintf(&msg, "[%02d/%02d %02d:%02d:%02d] %s:%u [%c] %s\n",
			tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
			file, line, LevelNames[(int)level][0], temp);
	free(temp);
	if (len < 0)
		throw std::bad_alloc();
	fputs(msg, stderr);
	free(msg);
}

void dumpData(const uint8_t *data, size_t len)
{
	for (size_t i = 0; i < len;)
	{
		char ascii[17] {};
		for (int j = 0; j < 16 && i + j < len; j++) {
			uint8_t b = data[i + j];
			fprintf(stderr, "%02x ", b);
			if (b >= ' ' && b < 0x7f)
				ascii[j] = (char)b;
			else
				ascii[j] = '.';
		}
		fprintf(stderr, "%s\n", ascii);
		i += 16;
	}
}
