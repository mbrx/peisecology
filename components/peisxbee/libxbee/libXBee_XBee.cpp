#include "libXBee_XBee.h"

#include <sstream>

#include <termios.h>

XBee::XBee(const char* device, unsigned int baudRate) :
XBeeSerialConn(device, baudRate) 
{
	
}

RawApiFrame XBee::readFrame() {
	return RawApiFrame::readFrame(*this);
}

