#pragma once

#include "libXBee_Defs.h"
#include "libXBee_XBeeSerialConn.h"
#include "libXBee_Error.h"

#include <string>

class RawApiFrame {
	static const byte CH_ESCAPE;
	static const byte CH_XON;
	static const byte CH_XOFF;
	static const byte CH_XOR_VAL;
	
	const byte_vec m_data;
	
	RawApiFrame(const byte_vec& data);
	
	static byte_vec unescape(const byte_vec& data);
public:
	
	static const byte CH_DELIMITER;
	
	RawApiFrame(const RawApiFrame& frame);
	
	static RawApiFrame readFrame(XBeeSerialConn& conn) throw(Error);
	
	size_t size() const;
	byte operator[] (unsigned idx) const;
	
	std::string toString() const;
	
	typedef byte_vec::const_iterator iterator;
	iterator begin() const;
	iterator end() const;
};

