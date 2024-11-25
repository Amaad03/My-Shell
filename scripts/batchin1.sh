echo ====================
echo PIPE TEST
echo ====================
cd pipe_tests
cat p1 | sort
echo ====================
echo MULTIPLE PIPE TEST
echo ====================
cat p1 | sort | uniq
echo ----------------
cat p1 | sort | cat | uniq