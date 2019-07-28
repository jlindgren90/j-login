CFLAGS = -Wall -O2 -std=c99 -D_POSIX_C_SOURCE=200112L $(shell pkg-config --cflags gtk+-2.0 x11)
LIBS = -lcrypt -lpam $(shell pkg-config --libs gtk+-2.0 x11)

SRCS = j-login.c pam.c screen.c utils.c
HDRS = pam.h screen.h utils.h

j-login : $(SRCS) $(HDRS) Makefile
	gcc ${CFLAGS} -o j-login ${SRCS} ${LIBS}

clean :
	rm -f j-login

uninstall :
	rm -f /usr/bin/j-session
	rm -f /usr/sbin/j-login
	rm -f /usr/sbin/j-login-setup
	rm -f /usr/sbin/j-login-sleep
	rm -f /lib/systemd/system/j-login.service
	rm -f /usr/share/pixmaps/j-login.png

install : uninstall
	cp j-session /usr/bin/
	cp j-login /usr/sbin/
	cp j-login-setup /usr/sbin/
	cp j-login-sleep /usr/sbin/
	cp j-login.service /lib/systemd/system/
	cp j-login.png /usr/share/pixmaps/
