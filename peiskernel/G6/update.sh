#!/bin/sh
svn update && ./configure && cd peiskernel && make && sudo make install
