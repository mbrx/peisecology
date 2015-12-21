#pragma once

#include "libXBee_Defs.h"
#include "libXBee_Error.h"
#include <deque>

#include <termios.h>


class XBeeSerialConn {
protected:
	std::deque<byte> m_peekedBytes;
	
	static const size_t LARGE_READ_WARNING_LIMIT;
	const unsigned m_flags;
	
	termios m_originalAttributes;
	
	int m_fd;
	
public:
	//Flags
	struct FLAGS {
		static const unsigned ESCAPED;
		static const unsigned DEFAULT;
	};
	
	XBeeSerialConn(const char* device, unsigned baudRate, unsigned flags = FLAGS::DEFAULT) throw(Error);
	~XBeeSerialConn() throw(Error);
	
	unsigned getFlags() const;
	
	bool is_open() const;
	operator bool () const;
	operator int ();
	
	byte peek();
	byte read();
	
	byte_vec peek(unsigned n);
	byte_vec read(unsigned n);
};
