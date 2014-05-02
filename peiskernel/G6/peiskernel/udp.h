/** \file udp.h
   Declares the UDP/IP linklayer interface
*/
/*
    Copyright (C) 2005 - 2012  Mathias Broxvall

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA
*/

#ifndef UDP_H
#define UDP_H

/** \ingroup LinkLayer */
/** \defgroup UdpIP  UDP/IPv4

    This is a linklayer interface using UDP over IP. Currently not in use. 

 */
/** @{ */

/** Checks for new incomming udp connections */
void peisk_acceptUDPConnections();                  
/** Listens for udp multicasts of available hosts on the local tcp/ip networks */
void peisk_acceptUDPMulticasts();                   

/** Performs the sending of data on a bluetooth L2CAP connection. Package
    points to a PeisPackage structure including data for a total 
    length of len bytes. Returns zero on success. */
int peisk_udpSendAtomic(PeisConnection *connection,PeisPackage *package,int len);

/** Attempts to read a package from the connection. 
    Returns non-zero if there was a package.  */
int peisk_udpReceiveIncomming(struct PeisConnection *connection,struct PeisPackage *package);

/* @} TCP/IPv4 */

#endif
