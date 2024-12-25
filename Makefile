# Compiler and flags
CC = gcc
CFLAGS = -Wall -g `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lm

# Output file
OUTPUT = main

# Source files
SRCS = main.c
OBJS = $(SRCS:.c=.o)

# Build and run target
all: $(OUTPUT)
	./$(OUTPUT)

$(OUTPUT): $(OBJS)
	$(CC) $(OBJS) -o $(OUTPUT) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(OUTPUT)

run: $(OUTPUT)
	./$(OUTPUT)
