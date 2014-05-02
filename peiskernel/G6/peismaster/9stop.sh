#!/bin/sh
##
## 9stop.sh
## 
## Made by Mathias Broxvall
## Login   <mbl@localhost>
## 
## Started on  Fri Mar  7 15:55:13 2008 Mathias Broxvall
## Last update Fri Mar  7 15:56:21 2008 Mathias Broxvall
##

echo "Stopping all the peismasters"
for i in `ps -ef | awk '$8 == "peismaster" {print $2}'` ; do echo "Stopping $i" ; kill  $i ; sleep 1 ; done
