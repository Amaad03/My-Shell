echo ====================
echo REDIRECT TESTS
echo ====================
mkdir redirect_tests
cd redirect_tests
echo foo bar > f0
echo foo > f1 bar
echo > f2 foo bar
cat < f0
cat < f0 > f3
cat > f4 < f0