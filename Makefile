#
# dependencies: libasio-dev libsqlite3-dev libcurl-dev
#
prefix = /usr/local
exec_prefix = $(prefix)
sbindir = $(exec_prefix)/sbin
sysconfdir = $(prefix)/etc
CFLAGS = -g -Wall -O3 # -DNDEBUG -fsanitize=address -static-libasan
CXXFLAGS = $(CFLAGS) -std=c++17
DEPS=asio.h tomcrypt.h http.h player.h log.h game.h db.h discord.h json.hpp Makefile
OBJS=http.o rc5.o player.o log.o server.o game.o db.o discord.o
USER = dcnet

all: afoserver

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

afoserver: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) -lpthread -lsqlite3 -lcurl

clean:
	rm -f $(OBJS) afoserver afo.service

install: all
	mkdir -p $(DESTDIR)$(sbindir)
	install afoserver $(DESTDIR)$(sbindir)
	mkdir -p $(DESTDIR)$(sysconfdir)
	cp -n afo.cfg $(DESTDIR)$(sysconfdir)

afo.service: afo.service.in Makefile
	cp afo.service.in afo.service
	sed -e "s/INSTALL_USER/$(USER)/g" -e "s:SBINDIR:$(sbindir):g" -e "s:SYSCONFDIR:$(sysconfdir):g" < $< > $@

installservice: afo.service
	mkdir -p /usr/lib/systemd/system/
	cp $< /usr/lib/systemd/system/
	systemctl enable afo.service

createdb:
	mkdir -p /var/lib/afo/
	chown $(USER):$(USER) /var/lib/afo
	sqlite3 /var/lib/afo/afo.db < createdb.sql
	chown $(USER):$(USER) /var/lib/afo/afo.db
