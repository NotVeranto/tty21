CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -g
VERSION = 0.1.0
CFLAGS += -DTTY21_VERSION=\"$(VERSION)\"
PREFIX  ?= /usr/local
DESTDIR ?=
BINDIR  = $(PREFIX)/bin
TARGET  = tty21
SRCS    = main.c card.c hand.c game.c render.c input.c dealers.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

.PHONY: all clean install uninstall
