CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -g
VERSION = 0.1.0
CFLAGS += -DTTY21_VERSION=\"$(VERSION)\"
CFLAGS += -MMD -MP
PREFIX  ?= /usr/local
DESTDIR ?=
BINDIR  = $(PREFIX)/bin
TARGET  = tty21
SRCS    = main.c card.c hand.c game.c round.c render.c menu.c input.c input_map.c dealers.c twists.c rng.c perks.c run.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) *.d

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

.PHONY: all clean install uninstall

-include $(OBJS:.o=.d)
