#!/usr/local/bin/tuplescript
sleep 5

echo "Stopping all PEIS-init components\n"
set 100:do-quit 'yes
set 101:kernel.do-quit 'yes
set 102:kernel.do-quit 'yes
set 103:kernel.do-quit 'yes


set 200:do-quit 'yes
set 201:kernel.do-quit 'yes
set 202:kernel.do-quit 'yes
set 203:kernel.do-quit 'yes

set 300:do-quit 'yes
set 301:kernel.do-quit 'yes

sleep 3
echo "Stopping the tuplemonitor"
set 1001:do-quit 'yes

sleep 10


