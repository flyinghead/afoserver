CFLAGS = -g -Wall # -O3 -DNDEBUG # -fsanitize=address -static-libasan
CXXFLAGS = $(CFLAGS) -std=c++17
DEPS=asio.h tomcrypt.h http.h player.h log.h Makefile
OBJS=http.o rc5.o player.o log.o server.o game.o

all: afoserver

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

afoserver: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) afoserver
