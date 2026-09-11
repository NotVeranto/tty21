CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11
TARGET  = tty21
SRCS    = main.c card.c hand.c game.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
