echo ====================
echo PIPE TEST WITH GREP
echo ====================
cd pipe_tests
cat p1 | grep "error" | sort
echo ====================
echo PIPE TEST WITH WORD COUNT
echo ====================
cat p1 | grep -v "error" | wc -l
echo ----------------
cat p1 | grep -o "[a-zA-Z]\+" | sort | uniq -c | sort -nr
echo ====================
echo COMPLEX PIPE TEST WITH MULTIPLE STEPS
echo ====================
cat p1 | tr '[:lower:]' '[:upper:]' | grep "WARNING" | sort | uniq -c | awk '{print $2 ": " $1}'
