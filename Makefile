CC = gcc
CFLAGS = -Wall -Wextra -lutil
TARGET = fssh
OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h);

.PHONY: all clean

all: $(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: all

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	$(RM) -f $(OBJECTS)
