#!/bin/bash
echo "Installing docs/man into /usr/local"
echo "Note that I assume that you have already generated them"
echo "by running doxygen doxygen.conf"
echo -n "installing ..."
mkdir -p /usr/local/man/man3
cp -r docs/man/man3/* /usr/local/man/man3/
sleep 1
echo "done"
echo "You can start browsing the manpages by doing one of:"
echo "man tuples"
echo "man peisk"
echo "man peiskmt"
