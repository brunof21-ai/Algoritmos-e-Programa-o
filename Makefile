CC = gcc
CFLAGS = -Wall `pkg-config --cflags gtk+-3.0 sqlite3`
LIBS = `pkg-config --libs gtk+-3.0 sqlite3`

SRCS = src/main.c src/db.c src/ui.c
OBJS = $(SRCS:.c=.o)
TARGET = sistema_doacoes

all: $(TARGET)

$(TARGET):$(OBJS)
	$(CC)$(OBJS) -o $(TARGET)$(LIBS)

%.o: %.c
	$(CC)$(CFLAGS) -c $< -o$@

clean:
	rm -f src/*.o $(TARGET) sistema_doacoes.db

.PHONY: all clean