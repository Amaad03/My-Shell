CC      = gcc
FLAGS   = -g -std=c99 -Wall -Wvla -Werror -fsanitize=address,undefined
TARGET  = mysh

# Default target: mysh
all: $(TARGET)

# Build target
$(TARGET): mysh.c
	$(CC) $(FLAGS) -o $@ $^

# Clean up build files
clean:
	rm -f $(TARGET) *.o
