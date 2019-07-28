CFLAGS = -Wall -O2 -std=c99 -D_POSIX_C_SOURCE=200112L $(shell pkg-config --cflags gtk+-2.0 x11)
LIBS = -lcrypt -lpam $(shell pkg-config --libs gtk+-2.0 x11)

SRCS = j-login.c pam.c screen.c utils.c
HDRS = pam.h screen.h utils.h

j-login : $(SRCS) $(HDRS) Makefile
	gcc ${CFLAGS} -o j-login ${SRCS} ${LIBS}

clean :
	rm -f j-login

uninstall :
	rm -f ${DESTDIR}/usr/bin/j-session
	rm -f ${DESTDIR}/usr/sbin/j-login
	rm -f ${DESTDIR}/usr/sbin/j-login-setup
	rm -f ${DESTDIR}/usr/sbin/j-login-sleep
	rm -f ${DESTDIR}/lib/systemd/system/j-login.service
	rm -f ${DESTDIR}/usr/share/pixmaps/j-login.png

install :
	mkdir -p ${DESTDIR}/usr/bin
	mkdir -p ${DESTDIR}/usr/sbin
	mkdir -p ${DESTDIR}/lib/systemd/system
	mkdir -p ${DESTDIR}/usr/share/pixmaps
	cp j-session ${DESTDIR}/usr/bin/
	cp j-login ${DESTDIR}/usr/sbin/
	cp j-login-setup ${DESTDIR}/usr/sbin/
	cp j-login-sleep ${DESTDIR}/usr/sbin/
	cp j-login.service ${DESTDIR}/lib/systemd/system/
	cp j-login.png ${DESTDIR}/usr/share/pixmaps/
