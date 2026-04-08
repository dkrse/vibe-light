CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -O2 \
         $(shell pkg-config --cflags libadwaita-1 vte-2.91-gtk4)
LDFLAGS = $(shell pkg-config --libs libadwaita-1 vte-2.91-gtk4) -lm

SRC = src/main.c src/window.c src/settings.c src/actions.c
OBJ = $(SRC:src/%.c=build/%.o)
TARGET = build/vibe-light

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f build/*.o $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
