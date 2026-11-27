CC      ?= cc
CFLAGS  ?= -O2
VERSION ?= 0.1.2
CPPFLAGS += -DTTY21_VERSION=\\\"$(VERSION)\\\"
TTY21_CFLAGS = -Wall -Wextra -Werror -std=c11 -MMD -MP
PREFIX  ?= /usr/local
DESTDIR ?=
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/share/man
TARGET  = tty21
SIMULATION_REPORT = tools/simulation_report
SRCS    = main.c card.c hand.c game.c controller.c round.c round_cards.c round_economy.c round_rules.c round_input.c round_actions.c round_dealer.c round_simulation.c round_presentation.c round_settlement.c render.c render_terminal.c render_table_info.c render_oracle.c render_messages.c render_status.c render_cards.c render_shop.c render_log.c render_fallback.c render_style.c render_canvas.c render_transitions.c render_menu.c render_actions.c render_stats.c render_help.c render_layout.c terminal.c menu.c input.c input_map.c dealers.c twists.c rng.c perks.c run.c save.c
OBJS    = $(SRCS:.c=.o)
TESTS_SRCS = tests/run_tests.c tests/test_hand.c \
             tests/test_card.c tests/test_rng.c tests/test_twists.c \
             tests/test_dealers.c tests/test_perks.c tests/test_run.c \
             tests/test_settlement.c tests/test_actions.c \
             tests/test_simulation.c tests/test_save.c tests/test_render.c
TEST_LIB_SRCS = $(filter-out main.c,$(SRCS))
CORE_LIB_SRCS = card.c hand.c game.c round_cards.c round_economy.c \
                round_rules.c round_settlement.c dealers.c twists.c rng.c \
                perks.c run.c

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(CPPFLAGS) $(CFLAGS) $(TTY21_CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TTY21_CFLAGS) -c $< -o $@

$(SIMULATION_REPORT): tools/simulation_report.c $(TEST_LIB_SRCS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter-out -MMD -MP,$(TTY21_CFLAGS)) -I. -o $@ $^

simulate-report: $(SIMULATION_REPORT)
	./$(SIMULATION_REPORT)

test: test-unit test-core test-smoke

test-unit: $(TESTS_SRCS) $(TEST_LIB_SRCS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter-out -MMD -MP,$(TTY21_CFLAGS)) -I. -o tests/tty21_tests $(TESTS_SRCS) $(TEST_LIB_SRCS)
	./tests/tty21_tests

test-core: tests/test_core.c $(CORE_LIB_SRCS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter-out -MMD -MP,$(TTY21_CFLAGS)) -I. -o tests/tty21_core_tests tests/test_core.c $(CORE_LIB_SRCS)
	./tests/tty21_core_tests

test-smoke: $(TARGET)
	sh tests/smoke_cli.sh
	python3 tests/smoke_terminal.py

test-sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='$(filter-out -MMD -MP,$(CFLAGS)) -fsanitize=address,undefined -fno-omit-frame-pointer' test-unit test-core

clean:
	rm -f $(OBJS) $(TARGET) *.d tests/tty21_tests tests/tty21_tests.d tests/tty21_core_tests $(SIMULATION_REPORT)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -Dm644 tty21.1 $(DESTDIR)$(MANDIR)/man1/tty21.1
	install -Dm644 README.md $(DESTDIR)$(PREFIX)/share/doc/tty21/README.md
	install -Dm644 LICENSE $(DESTDIR)$(PREFIX)/share/licenses/tty21/LICENSE

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET) $(DESTDIR)$(MANDIR)/man1/tty21.1

.PHONY: all test test-unit test-core test-smoke test-sanitize simulate-report clean install uninstall

-include $(OBJS:.o=.d)
