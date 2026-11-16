CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -g
VERSION = 0.1.0
CFLAGS += -DTTY21_VERSION=\"$(VERSION)\"
CFLAGS += -MMD -MP
PREFIX  ?= /usr/local
DESTDIR ?=
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/share/man
TARGET  = tty21
SRCS    = main.c card.c hand.c game.c controller.c round.c round_cards.c round_economy.c round_rules.c round_input.c round_actions.c round_dealer.c round_settlement.c render.c render_menu.c render_actions.c render_stats.c render_help.c render_layout.c terminal.c menu.c input.c input_map.c dealers.c twists.c rng.c perks.c run.c save.c
OBJS    = $(SRCS:.c=.o)
TESTS_SRCS = tests/run_tests.c tests/test_hand.c \
             tests/test_card.c tests/test_rng.c tests/test_twists.c \
             tests/test_dealers.c tests/test_perks.c tests/test_run.c \
             tests/test_settlement.c tests/test_save.c \
             tests/test_render.c
TEST_LIB_SRCS = $(filter-out main.c,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: test-unit test-smoke

test-unit: $(TESTS_SRCS) $(TEST_LIB_SRCS)
	$(CC) $(filter-out -MMD -MP,$(CFLAGS)) -I. -o tests/tty21_tests $(TESTS_SRCS) $(TEST_LIB_SRCS)
	./tests/tty21_tests

test-smoke: $(TARGET)
	sh tests/smoke_cli.sh

test-sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='$(filter-out -MMD -MP,$(CFLAGS)) -fsanitize=address,undefined -fno-omit-frame-pointer' test-unit

clean:
	rm -f $(OBJS) $(TARGET) *.d tests/tty21_tests tests/tty21_tests.d

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -Dm644 tty21.1 $(DESTDIR)$(MANDIR)/man1/tty21.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET) $(DESTDIR)$(MANDIR)/man1/tty21.1

.PHONY: all test test-unit test-smoke test-sanitize clean install uninstall

-include $(OBJS:.o=.d)
