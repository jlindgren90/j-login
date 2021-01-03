BASE_CFLAGS = -Wall -Wextra -O2 -std=c99 -D_GNU_SOURCE
CFLAGS = ${BASE_CFLAGS} $(shell pkg-config --cflags gtk+-2.0 x11) -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32
LIBS = -lcrypt -lpam $(shell pkg-config --libs gtk+-2.0 x11) -lXss

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
	rm -f ${DESTDIR}/usr/lib/udev/rules.d/60-j-login.rules
	rm -f ${DESTDIR}/usr/share/pixmaps/j-login.png

install :
	install -d ${DESTDIR}/usr/bin
	install -d ${DESTDIR}/usr/lib/systemd/system
	install -d ${DESTDIR}/usr/lib/udev/rules.d
	install -d ${DESTDIR}/usr/share/pixmaps
	install j-login ${DESTDIR}/usr/bin/
	install j-login-lock ${DESTDIR}/usr/bin/
	chmod +s ${DESTDIR}/usr/bin/j-login-lock
	install j-login-setup ${DESTDIR}/usr/bin/
	install j-login-sleep ${DESTDIR}/usr/bin/
	install j-session ${DESTDIR}/usr/bin/
	install -m644 j-login.service ${DESTDIR}/usr/lib/systemd/system/
	install -m644 j-login-sleep.service ${DESTDIR}/usr/lib/systemd/system/
	install -m644 60-j-login.rules ${DESTDIR}/usr/lib/udev/rules.d/
	install -m644 j-login.png ${DESTDIR}/usr/share/pixmaps/
