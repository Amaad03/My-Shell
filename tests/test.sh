#!/bin/bash

# Test 1: Simple echo command
echo "Running Test 1: Simple echo"
echo "Hello World" | ./mysh
echo

# Test 2: Invalid command
echo "Running Test 2: Invalid command"
./mysh fakecommand
echo

# Test 3: Wildcard expansion (ensure there's at least one .c file)
echo "Running Test 3: Wildcard expansion"
echo "*.c" | ./mysh
echo

# Test 4: Built-in command exit
echo "Running Test 4: Exit command"
echo "exit" | ./mysh
echo

# Test 5: Command with background process
echo "Running Test 5: Background process"
echo "sleep 5 &" | ./mysh
echo

# Add more test cases as needed...
