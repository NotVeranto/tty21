CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -g
VERSION = 0.1.0
CFLAGS += -DTTY21_VERSION=\"$(VERSION)\"
CFLAGS += -MMD -MP
PREFIX  ?= /usr/local
DESTDIR ?=
BINDIR  = $(PREFIX)/bin
TARGET  = tty21
SRCS    = main.c card.c hand.c game.c round.c round_cards.c round_economy.c round_rules.c round_input.c round_actions.c render.c menu.c input.c input_map.c dealers.c twists.c rng.c perks.c run.c save.c
OBJS    = $(SRCS:.c=.o)
TESTS_SRCS = tests/run_tests.c tests/test_hand.c \
             tests/test_card.c tests/test_rng.c tests/test_twists.c \
             tests/test_dealers.c tests/test_perks.c tests/test_run.c
LIB_SRCS = card.c hand.c dealers.c twists.c perks.c rng.c run.c

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TESTS_SRCS) $(LIB_SRCS)
	$(CC) $(filter-out -MMD -MP,$(CFLAGS)) -I. -o tests/tty21_tests $(TESTS_SRCS) $(LIB_SRCS)
	./tests/tty21_tests

clean:
	rm -f $(OBJS) $(TARGET) *.d tests/tty21_tests tests/tty21_tests.d

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

.PHONY: all test clean install uninstall

-include $(OBJS:.o=.d)
