TARGET ?= writer

SRCS := writer.c
OBJS := $(SRCS:.c=.o)

CC := $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -O2

.PHONY: all default clean

all: default

default: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) 
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(TARGET) $(OBJS) *.o
