#pragma once

#include "libXBee_XBeeSerialConn.h"
#include "libXBee_RawApiFrame.h"

class XBee : public XBeeSerialConn {
	
public:
	XBee(const char* device, unsigned int baudRate);
	RawApiFrame readFrame();
};
