BASE_CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200112L
CFLAGS = ${BASE_CFLAGS} $(shell pkg-config --cflags gtk+-2.0 x11)
LIBS = -lcrypt -lpam $(shell pkg-config --libs gtk+-2.0 x11)

SRCS = j-login.c pam.c screen.c ui.c utils.c
HDRS = actions.h pam.h screen.h ui.h utils.h

all : j-login j-login-lock

j-login : $(SRCS) $(HDRS) Makefile
	gcc ${CFLAGS} -o j-login ${SRCS} ${LIBS}

j-login-lock : j-login-lock.c Makefile
	gcc ${BASE_CFLAGS} -o j-login-lock j-login-lock.c

clean :
	rm -f j-login j-login-lock

uninstall :
	rm -f ${DESTDIR}/usr/bin/j-login
	rm -f ${DESTDIR}/usr/bin/j-login-lock
	rm -f ${DESTDIR}/usr/bin/j-login-setup
	rm -f ${DESTDIR}/usr/bin/j-login-sleep
	rm -f ${DESTDIR}/usr/bin/j-session
	rm -f ${DESTDIR}/usr/lib/systemd/system/j-login.service
	rm -f ${DESTDIR}/usr/lib/systemd/system/j-login-sleep.service
	rm -f ${DESTDIR}/usr/share/pixmaps/j-login.png

install :
	mkdir -p ${DESTDIR}/usr/bin
	mkdir -p ${DESTDIR}/usr/lib/systemd/system
	mkdir -p ${DESTDIR}/usr/share/pixmaps
	cp j-login ${DESTDIR}/usr/bin/
	cp j-login-lock ${DESTDIR}/usr/bin/
	chmod +s ${DESTDIR}/usr/bin/j-login-lock
	cp j-login-setup ${DESTDIR}/usr/bin/
	cp j-login-sleep ${DESTDIR}/usr/bin/
	cp j-session ${DESTDIR}/usr/bin/
	cp j-login.service ${DESTDIR}/usr/lib/systemd/system/
	cp j-login-sleep.service ${DESTDIR}/usr/lib/systemd/system/
	cp j-login.png ${DESTDIR}/usr/share/pixmaps/
