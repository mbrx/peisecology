#!/usr/local/bin/tuplescript
# Tuplescript that setups up the configuration in the experiment

sleep 3

echo "Configuring experiment"
set 100:settings "beam me up scotty!"
set_meta 102:mi-laser 100:laser
set_meta 102:mi-odometry 101:odometry

echo "Finished"
sleep 1
exit
