echo --------------------
echo Redirect Case 
echo --------------------
mkdir Redirect_Case
cd Redirect_Case
echo foo > f1
echo bar > f0 foo
echo > f2 foo bar
cat < f2
cat < f1 > f4
cat > f3 < f0