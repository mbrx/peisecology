PEIS Ecology
============

The PEIS Ecology project is a research project that started 2004 at
the Centre for Applied Autonomous Sensor Systems. Several European FP7
projects have built on the infrastructure provided by this project and
some of the softwares developed within the original PEIS Ecology
project have there been extended and further maintained.

This git repository is a public release of the latest state of many of
the softwares developed within these projects and although not
maintained and in many cases not usable without very specific hardware
setups this software is released in the hope of beeing useful for
other researchers or hobyists.

Apart from where otherwise noted the softwares within these archives
is released under the GNU GENERAL PUBLIC LICENSE Version 2 (GPLv2). 

Brief concept of PEIS Ecologies
===============================

 - A common vision in the field of autonomous robotics is to create a
   skilled robot companion that is able to live in our homes and
   workplaces, and to perform physical tasks for helping us in our
   everyday life.

 - Another vision coming from the field of ambient intelligence, is to
   create a network of intelligent home devices that provide us with
   information, communication, and entertainment.

 - This project aims at combining these two visions into the new
   concept of an Ecology of networked Physically Embedded Intelligent
   Systems, or PEIS (pronounced [peIs] like in 'pace'). The
   PEIS-Ecology will provide cognitive and physical assistance to the
   citizens of the future, and help them to live a better, safer and
   more independent life.

The PEIS-Ecology approach belongs to a trend which is becoming rather
popular in the area of home and service robotics: to abandon the idea
of having one extremely competent isolated robot acting in a passive
environment, in favor of a network of cooperating robotic devices
embedded in the environment. In the PEIS-Ecology approach, advanced
robotic functionalities are not achieved by the development of
extremely advanced robots, but through the cooperation of many simple
robotic components.

The PEIS-Ecology approach takes an ecological view of the
robot-environment relationship, in which the robot and the environment
are seen as parts of the same system, engaged in a symbiotic
relationship toward the achievement of a common goal, or equilibrium
status. We assume that robotic devices (or PEIS, for Physically
Embedded Intelligent Systems)  are pervasively distributed throughout
the working space in the form of sensors, actuators, smart appliances,
RFID-tagged objects, or more traditional mobile robots; and that these
PEIS can communicate and collaborate with each other by providing
information and by performing actions. Humans can also be included in
this approach as another species of PEIS inside the same ecosystem.

Overview
========

From a pragmatical point of view, the implementation of PEIS Ecologies
requires both a set of infrastructure tools and standardized methods
for communication, as well as a vast set of specific programs
"components" running on the different pieces of hardware that was
built up for this project. These can be classified as follows:

 - The PEIS Kernel

 - Utility programs: tupleview, peisinit

 - PEIS Components: all other programs that perform actuation/sensing
   or modelling/deliberation of data.

The PEIS kernel
---------------

This is a very small software library with a small memory and CPU
footprint portable to almost all posix compliant systems. This library
provides the basic communication mechanism which the different
subsystems of a PEIS Ecology uses for all communication.

The library will connect all devices that are reachable on any found
local networks, and when relevant connect different local networks, in
a peer-to-peer (P2P) network. On top of this P2P network a number of
services is provided, the most important of which is the
implementation of a _distributed tuplespace_. Another service is to
assign unique integer ID numbers to all running programs (PEIS-ID)
which is used in the addressing scheme.

The distributed tuplespace is a (non-persistent) key-value database
which is used as a means of communication. The distribution mechanism
depends heavily on the addressing scheme for keys and ensures that
data are only sent to relevant devices and that any part of the
network can temporarily leave the P2P network and rejoin
seamlessly. The distribution mechanism is based on partitioning the
allowed namespace of keys to a format of <ID>.<string>(.<string>)* and
where the first ID part of each such key decides the component that
maintains a master copy of the latest value for that key.

Tupleview
---------

Tupleview is a program for inspecting (and/or modifying) the latest
tuple values in any of the different components running on the
network, as well as for performing various network diagnostics.

PeisInit
--------

PeisInit can be seen as a PEIS equivalent of the inetd services of
classical unix systems. It performs the job of starting/stopping the
relevant components (installed softwares) available on machines in the
network. Although the job of PEIS Init can be performed manually by
logging in through SSH this provides a simpler and safer mechanism for
starting, monitoring stdin/stdout/aliveness and termining the
processes for any PEIS component on any of the machines in the PEIS
network.


INSTALLING
==========

Peiskernel
----------

Compiling the peiskernel requires some dependencies - assuming an
Ubuntu-like distro:

> sudo apt-get install libtool
> sudo apt-get install autotools-dev

To compile, run some configuration scripts first:

> cd peiskernel/G6
> ./autogen.sh
> ./configure

Then, compile and install:

> make
> sudo make install

To let your system know of the new library in the current session, you
can:

> sudo ldconfig

To test, run the peismaster:

> peismaster


Tupleview
---------

For tupleview, you need the following dependencies:

> sudo apt-get install libglade2-dev

To install:

> cd tupleview/G6
> ./autogen.sh
> make
> sudo make install

To test, run a peismaster and start tupleview:

> peismaster & tupleview

You should see the peismaster component in tupleview.


PEISJava
--------

You need the ant build system to make and install the PEISJava binding:

> sudo apt-get install ant

To make install:

> cd bindings/PEISJava/G6.1
> ant all
> sudo ant install

To test, run:

> java -jar dist/PeisJava.jar


Documentation
=============

To generate API docs, use doxygen:

> cd peiskernel/G6
> doxygen doxygen.conf

You will now find the HTML docs in peiskernel/G6/docs/html/index.html

After having generated docs with doxygen, yuo can install man pages:

> cd peiskernel/G6
> sudo ./install-manpages.sh