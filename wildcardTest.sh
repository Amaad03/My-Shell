echo ====================
echo WILDCARD TEST: LIST ALL .TXT FILES
echo ====================
cd wildcard_tests
ls *.txt
echo ====================
echo WILDCARD TEST: DISPLAY CONTENT OF FILES STARTING WITH 'test'
echo ====================
cat test*.txt
echo ----------------
echo WILDCARD TEST: SORT AND DISPLAY UNIQUE WORDS FROM ALL '.log' FILES
echo ====================
cat *.log | tr '[:space:]' '\n' | sort | uniq
echo ----------------
echo WILDCARD TEST: COUNT FILES WITH SPECIFIC PATTERN
echo ====================
ls report_*.csv | wc -l
