#!/bin/sh
##
## post-install.sh
## 
## Made by Mathias Broxvall
## Login   <rlh@localhost>
## 
## Started on  Tue Dec 18 19:06:21 2007 Mathias Broxvall
## Last update Tue Dec 18 19:10:32 2007 Mathias Broxvall
##

echo ""
echo "*** PEISINIT - Post Install ***"
echo ""

SHARE=/usr/local/share/peisinit
SOURCE=`hostname`
if [ -d $SHARE ] ; then
  echo "$SHARE already exists";
else
  echo "Creating $SHARE"
  mkdir $SHARE
  chmod a+rw $SHARE;
fi
  
chown peis /usr/local/bin/peisinit
chmod +s /usr/local/bin/peisinit
chown peis /usr/local/share/peisinit
chmod a+rw /usr/local/share/peisinit

if [ -d $SOURCE ] ; then
  echo "Installing components from $SOURCE"
  for i in $SOURCE/* ; do
    echo "Installing $i"
    cp $i $SHARE/
  done
  echo "Making all log files world read/writable"
  chmod a+rw $SHARE/*.std{in,err,out,log}
  chmod a+rw $SHARE/peisinit.lock
else
  echo "No components for $SOURCE available."
  echo "Please create components manually and commit to SVN"
fi
 