CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
TARGET = image_toolkit

SRC = main.c bmp_helpers.c bmp_features.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c bmp_converter.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
