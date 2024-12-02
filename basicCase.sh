echo --------------------
echo Basic Test
echo --------------------
mkdir test_directory
cd test_directory
pwd
touch test.c
ls *.c
cd ..
pwd
cd ./test_directory/../
ls

echo --------------------
echo Which Command
echo --------------------
which ls
which which
which cd
which
echo Exiting the Shell
exit
