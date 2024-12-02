CC      = gcc
FLAGS   = -g -std=c99 -Wall -Wvla -Werror -fsanitize=address,undefined
TARGET  = mysh

all: $(TARGET)

# Build target
$(TARGET): mysh.c
	$(CC) $(FLAGS) -o $@ $^


clean:
	rm -f $(TARGET) *.o
