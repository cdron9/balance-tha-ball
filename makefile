CC = gcc

CFLAGS = -Wall -Wextra -I/opt/homebrew/include

LDFLAGS = -L/opt/homebrew/lib -lSDL3

SRC = main.c parse.c cJSON.c

TARGET = control

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)