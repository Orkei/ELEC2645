# Bash shell script to run tests. 
# Runs the test and then compares the output to what is expected
#
# Note to students: You dont need to fully understand this! 

echo "#####################################"
echo "       d[o_0]b ELEC2645 Bot          "
echo "#####################################"
echo "Running tests..."
echo

echo "Checking your code compiles..."


failed=0

# Fail if the executable doesn't exist or isn't runnable
if [ ! -f ./main.out ]; then
  echo "Fail: ./main.out not found"
  failed=1
fi

if [ ! -x ./main.out ]; then
  echo "Fail: ./main.out is not executable"
  failed=1
fi

echo
echo "Just checking the file compiled successfully, no further tests of functionality"
echo "This is the only automated check, the rest of your project will be marked manually."


echo
if [ $failed -eq 0 ]; then
  echo "#####################################"
  echo "     d[o_0]b All tests passed :D    "
  echo "#####################################"
  exit 0
else
  echo "#####################################"
  echo "     d[o_0]b Some tests failed :(   "
  echo "#####################################"
  exit 1
fi