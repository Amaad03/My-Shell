#!/bin/bash

echo ===========================
echo TESTING BASIC SHELL COMMANDS
echo ===========================

echo "Current Directory:"
pwd

echo "Changing to 'scripts' directory:"
cd scripts

echo "Current Directory after cd:"
pwd

echo "Listing contents of 'scripts' directory:"
ls

echo "Returning to parent directory:"
cd ..

echo "Current Directory after returning:"
pwd

echo "Creating a new directory 'testdir':"
mkdir testdir

echo "Listing contents of parent directory:"
ls

echo "Changing to 'testdir' directory:"
cd testdir

echo "Current Directory after creating and entering 'testdir':"
pwd

echo "Creating a new file 'example.txt':"
touch example.txt

echo "Listing contents of 'testdir':"
ls

echo "Removing 'example.txt':"
rm example.txt

echo "Listing contents of 'testdir' after removing the file:"
ls

echo "Returning to parent directory:"
cd ..

echo "Removing 'testdir':"
rmdir testdir

echo "Final Directory Listing:"
ls