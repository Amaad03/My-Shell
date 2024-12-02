echo ====================
echo WILDCARD TEST: LIST ALL .TXT FILES
echo ====================
mkdir wildcard_files
pwd
cd wildcard_files
touch a.txt
touch d.txt
touch c.txt
ls *.txt
echo *.txt | wc -w
echo ====================
echo WILDCARD TEST: DISPLAY CONTENT OF FILES STARTING WITH 'test'
echo ====================
touch testfoo.c
touch testfile.c
touch footest.c
echo test*.c
echo ====================
echo WILDCARD TEST: COUNT FILES WITH SPECIFIC PATTERN
echo ====================
touch report_2024.csv
touch report_3.csv
ls report_*.csv
echo ====================
echo report_*.csv | wc -l
