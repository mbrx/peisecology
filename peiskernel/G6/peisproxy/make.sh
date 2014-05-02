#!/bin/bash

export LD_LIBRARY_PATH="/usr/local/lib/"

cd ~/svnroot/peis/G4/peisproxy
make && sudo make install

cd ~/svnroot/peistools/proxy_proxied_concept/twoIfaceProxy/trunk/src
make && sudo make install

#twoIfaceProxy --peis-id 2000
