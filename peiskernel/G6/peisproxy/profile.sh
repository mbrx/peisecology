#!/bin/bash
echo "First run twoIface proxy which will generate gmon.out"
echo "then run this script"
#gprof twoIfaceProxy | ./gprof2dot.py  -n 1 | dot -T png > tmp.png

gprof proxytest | ./gprof2dot.py  -n 1 | dot -T png > tmp.png

echo "ok, now look at the image tmp.png"
