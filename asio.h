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
#include <asio.hpp>

#undef ECHO

#if ASIO_VERSION < 102900
namespace asio::placeholders
{
static inline constexpr auto& error = std::placeholders::_1;
static inline constexpr auto& bytes_transferred = std::placeholders::_2;
}
#endif

class DynamicBuffer
{
public:
	using const_buffers_type = asio::ASIO_CONST_BUFFER;
	using mutable_buffers_type = asio::ASIO_MUTABLE_BUFFER;

	size_t size() const { return dynBuffer.size(); }
	size_t max_size() const { return dynBuffer.max_size(); }
	size_t capacity() const { return dynBuffer.capacity(); }
	const_buffers_type data() const { return dynBuffer.data(); }
	mutable_buffers_type prepare(size_t size) { return dynBuffer.prepare(size); }
	void commit(size_t n) { dynBuffer.commit(n); }
	void consume(size_t n) { dynBuffer.consume(n); }
	const uint8_t *bytes() const { return &vector[0]; }

private:
	std::vector<uint8_t> vector;
	asio::dynamic_vector_buffer<uint8_t, std::vector<uint8_t>::allocator_type> dynBuffer{vector};
};
