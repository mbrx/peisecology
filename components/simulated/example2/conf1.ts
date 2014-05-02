#!/usr/local/bin/tuplescript
sleep 3

set 100:components.odometry.reqState 'on
set 100:components.laser.reqState 'on
set 100:components.navigator.reqState 'on
set 300:components.slam.reqState 'on

sleep 5
set 102:settings 'foo
set_meta 103:mi-odometry 101:odometry
set_meta 103:mi-localization 301:localization

set_meta 301:mi-odometry 101:odometry
set_meta 301:mi-rangeReadings 102:laser

sleep 3


