CC ?= gcc
CFLAGS ?= -Wall -Werror -g
TARGET = aesdsocket
SRC = aesdsocket.c
OBJS = $(SRC:.c=.o)
all: default
default: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) /var/tmp/aesdsocketdata

.PHONY: all default clean
