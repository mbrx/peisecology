#!/bin/bash

NETWORK=rubicon
#NETWORK=default

peisinit --peis-network $NETWORK --initdir robot-1 --peis-hostname robot-1 --peis-id 100 2&> robot-1.log &
peisinit --peis-network $NETWORK --initdir robot-2 --peis-hostname robot-2 --peis-id 200 2&> robot-2.log &
peisinit --peis-network $NETWORK --initdir server --peis-hostname server  --peis-id 300 2&> server.log &

tuplemonitor --peis-network $NETWORK --peis-id 1001 \
    --monitor ReqOdometry 100 components.odometry.reqState \
    --monitor CurOdometry 100 components.odometry.currState \
    --monitor Odometry 101 status \
    --monitor ReqLaser 100 components.laser.reqState \
    --monitor CurLaser 100 components.laser.currState \
    --monitor ReqLaser 102 status \
    --monitor ReqNavigator 100 components.navigator.reqState \
    --monitor CurNavigator 100 components.navigator.currState \
    --monitor Navigator 103 status \
    --monitor ReqOdometry2 200 components.odometry.reqState \
    --monitor CurOdometry2 200 components.odometry.currState \
    --monitor Odometry2 201 status \
    --monitor ReqKinnect 200 components.kinnect.reqState \
    --monitor CurKinnect 200 components.kinnect.currState \
    --monitor Kinnect 202 status \
    --monitor ReqSLAM 300 components.slam.reqState \
    --monitor CurSLAM 300 components.slam.currState \
    --monitor SLAM 301 status \
    --monitor SLAM-mkerror 301 provoke-error \
    > /dev/null &
