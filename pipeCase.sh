echo ====================
echo PIPE TEST WITH GREP
echo ====================
cd pipe_tests
cat f1 | sort
echo ====================
echo PIPE TEST WITH WORD COUNT
echo ====================
cat f1 | grep -v "error" | wc -l
echo ----------------
cat f1 | grep -o "[a-zA-Z]\+" | sort | uniq -c | sort -nr
echo ====================
echo COMPLEX PIPE TEST WITH MULTIPLE STEPS
echo ====================
cat f1 | sort | cat | uniq