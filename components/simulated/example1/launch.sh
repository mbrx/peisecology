#!/bin/bash
echo "starting fake-laser, fake-odometry and fake-slam"
sim_generic --peis-id 100 --peis-name fake-laser --output laser 1.0  --parameter settings &
sim_generic --peis-id 101 --peis-name fake-odometry --output odometry 0.5 &
sim_generic --peis-id 102 --peis-name fake-slam --input mi-laser 2.0 --input mi-odometry 2.0 &