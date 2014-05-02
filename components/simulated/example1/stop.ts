#!/usr/local/bin/tuplescript
# Tuplescript that stops the experiment
sleep 3

echo "Stopping experiment\n"
set 100:kernel.do-quit "yes"
set 101:kernel.do-quit "yes"
set 102:kernel.do-quit "yes"

sleep 5
exit
