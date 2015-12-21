#include "libXBee_XBeeSerialConn.h"

#include <termios.h>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>

#include "libXBee_Error.h"

#include <cerrno>
#include <iostream>
#include <algorithm>

const size_t XBeeSerialConn::LARGE_READ_WARNING_LIMIT = 256;

template <unsigned _n>
struct bit { enum {n = (1 << _n)}; };


const unsigned XBeeSerialConn::FLAGS::ESCAPED = bit<0>::n;
const unsigned XBeeSerialConn::FLAGS::DEFAULT = 0;


speed_t getSpeed(unsigned baudRate) {
	static const std::pair<unsigned, speed_t> baudRatesToSpeed[] = {
		std::pair<unsigned,speed_t>(50, B50),
		std::pair<unsigned,speed_t>(75, B75),
		std::pair<unsigned,speed_t>(110, B110),
		std::pair<unsigned,speed_t>(134, B134),
		std::pair<unsigned,speed_t>(150, B150),
		std::pair<unsigned,speed_t>(200, B200),
		std::pair<unsigned,speed_t>(300, B300),
		std::pair<unsigned,speed_t>(600, B600),
		std::pair<unsigned,speed_t>(1200, B1200),
		std::pair<unsigned,speed_t>(1800, B1800),
		std::pair<unsigned,speed_t>(2400, B2400),
		std::pair<unsigned,speed_t>(4800, B4800),
		std::pair<unsigned,speed_t>(9600, B9600),
		std::pair<unsigned,speed_t>(19200, B19200),
		std::pair<unsigned,speed_t>(38400, B38400),
		std::pair<unsigned,speed_t>(57600, B57600),
		std::pair<unsigned,speed_t>(115200, B115200),
		std::pair<unsigned,speed_t>(230400, B230400),
		std::pair<unsigned,speed_t>(460800, B460800),
		std::pair<unsigned,speed_t>(500000, B500000),
		std::pair<unsigned,speed_t>(576000, B576000),
		std::pair<unsigned,speed_t>(921600, B921600),
		std::pair<unsigned,speed_t>(1000000, B1000000),
		std::pair<unsigned,speed_t>(1152000, B1152000),
		std::pair<unsigned,speed_t>(1500000, B1500000),
		std::pair<unsigned,speed_t>(2000000, B2000000),
		std::pair<unsigned,speed_t>(2500000, B2500000),
		std::pair<unsigned,speed_t>(3000000, B3000000),
		std::pair<unsigned,speed_t>(3500000, B3500000),
		std::pair<unsigned,speed_t>(4000000, B4000000),
	};
	
	
	for(unsigned i = 0; i < sizeof(baudRatesToSpeed)/ sizeof(*baudRatesToSpeed); ++i ) {
		if(baudRatesToSpeed[i].first == baudRate) {
			return baudRatesToSpeed[i].second;
		}
	}
	
	return B0;
}

XBeeSerialConn::XBeeSerialConn(const char* device, unsigned baudRate, unsigned flags) throw(Error) :
 m_flags(flags)
{
	
	m_fd = ::open(device, O_RDONLY | O_NOCTTY /*| O_NONBLOCK*/);
	
	if(m_fd < 0) {
		throw Error(std::string("Failed to open device ") + device);
	}
	
	if(tcgetattr(m_fd, &m_originalAttributes) != 0) {
		throw Error("Failed to retrieve original attributes");
	}
	
	termios new_attributes;
	std::memset(&new_attributes, 0, sizeof(new_attributes));
	
	const speed_t speed = getSpeed(baudRate);
	if(speed == B0) {
		std::stringstream ss;
		ss << "Invalid baud rate: " << baudRate;
		throw Error(ss.str());
	}
	
	if(cfsetispeed(&new_attributes, speed) != 0) {
		std::stringstream ss;
		ss << "Failed to set speed to: " << baudRate;
		throw Error(ss.str());
	}
	
	fcntl(m_fd, F_SETFL, 0);
	//fcntl(m_fd, F_SETFL, F_WRLCK);
	
	//Control mode flags
	new_attributes.c_cflag |= CS8;              // 8 data bit
	new_attributes.c_cflag |= CREAD;            // Enable receiver
	new_attributes.c_cflag |= CLOCAL;
	
	
	new_attributes.c_iflag = 0;
	
	new_attributes.c_oflag = 0;
	
	//Control characters
	new_attributes.c_cc[VTIME] = 5; //Block for 5*0.1 = 0.5 seconds
	new_attributes.c_cc[VMIN] = 0;
	
	
	if(tcflush(m_fd, TCIOFLUSH) != 0) {
		throw Error("Failed to flush");
	}
	
	if(tcsetattr(m_fd, TCSANOW, &new_attributes) != 0) {
		close(m_fd);
		throw Error("Failed to set new attributes");
	}
}

XBeeSerialConn::~XBeeSerialConn() throw(Error) {
	
	if(is_open()) {
		if(tcsetattr(m_fd, TCSANOW, &m_originalAttributes) != 0) {
			throw Error("Failed to restore original attributes");
		}
		close(m_fd);
	}
}

unsigned XBeeSerialConn::getFlags() const {
	return m_flags;
}

	
bool XBeeSerialConn::is_open() const {
	return m_fd >= 0;
}

XBeeSerialConn::operator bool () const {
	return is_open();
}

XBeeSerialConn::operator int () {
	return m_fd;
}

unsigned char XBeeSerialConn::peek() {
	return peek(1)[0];
}

unsigned char XBeeSerialConn::read() {
	return read(1)[0];
}

byte_vec XBeeSerialConn::peek(unsigned n) {
	byte_vec bytes = read(n);
	m_peekedBytes.insert(m_peekedBytes.begin(), bytes.begin(), bytes.end());
	return bytes;
}

byte_vec XBeeSerialConn::read(unsigned n) {
	
	if(n > LARGE_READ_WARNING_LIMIT) {
		std::clog << "Warning, performing large read of " << n << " bytes" << std::endl;
	}
	
	byte_vec bytes(n);
	size_t nRead = 0;
	
	//First take from the peek buffer
	while(!m_peekedBytes.empty() && nRead < n) {
		bytes[nRead++] = m_peekedBytes.at(0);
		m_peekedBytes.pop_front();
	}
	
	//Then read from the serial line
	while(nRead < n) {
		ssize_t readNow = ::read(m_fd, &bytes[nRead], n - nRead);
		
		if(readNow < 0) {
			throw Error(strerror(errno));
		} else {
			nRead += readNow;
		}
	}
	
	return bytes;
}
	
