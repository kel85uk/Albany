
cd /home/ikalash/nightlyCDash
grep "Test   #" nightly_log_functor_openMP.txt >& results0
grep "Test  #" nightly_log_functor_openMP.txt >& results1 
cat results0 results1 >& results11
grep "Test #" nightly_log_functor_openMP.txt >& results0
cat results11 results0 >& results1
grep " tests failed" nightly_log_functor_openMP.txt >& results2 
cat results1 results2 >& results3
grep "Total Test" nightly_log_functor_openMP.txt >& results4
cat results3 results4 >& results5
echo "" >> results5
grep "(Failed)" nightly_log_functor_openMP.txt >& results6 
cat results5 results6 >& results7
grep "\*Timeout" nightly_log_functor_openMP.txt >& results8
cat results7 results8 >& results_functor_openMP
echo "" >> results_functor_openMP
echo "The Albany CDash site can be accessed here: http://cdash.sandia.gov/CDash-2-3-0/index.php?project=Albany" >> results_functor_openMP
echo "" >> results_functor_openMP
rm results0 results1 results11 results2 results3 results4 results5 results6 results7 results8
