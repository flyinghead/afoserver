#
# dependencies: libasio-dev libsqlite3-dev
#
CFLAGS = -g -Wall # -O3 -DNDEBUG # -fsanitize=address -static-libasan
CXXFLAGS = $(CFLAGS) -std=c++17
DEPS=asio.h tomcrypt.h http.h player.h log.h db.h Makefile
OBJS=http.o rc5.o player.o log.o server.o game.o db.o

all: afoserver

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

afoserver: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) -lsqlite3

clean:
	rm -f $(OBJS) afoserver
