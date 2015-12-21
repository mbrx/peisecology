#include "libXBee_RawApiFrame.h"
#include "libXBee_Error.h"
#include <iostream>
#include <sstream>


const byte RawApiFrame::CH_DELIMITER = 0x7e;
const byte RawApiFrame::CH_ESCAPE = 0x7d;
const byte RawApiFrame::CH_XON = 0x11;
const byte RawApiFrame::CH_XOFF = 0x13;
const byte RawApiFrame::CH_XOR_VAL = 0x20;


RawApiFrame::RawApiFrame(const byte_vec& data) :
m_data(data)
{
	
}

//This function is untested
std::vector<byte> RawApiFrame::unescape(const byte_vec& data) {
	std::vector<byte> unescaped;
	unescaped.reserve(data.size());
	
	for(unsigned i = 0; i < data.size(); i++) {
		if(data[i] == CH_ESCAPE) {
			unescaped.push_back(data[++i] ^ CH_XOR_VAL);
		} else {
			unescaped.push_back(data[i]);
		}
	}
	
	return unescaped;
}
	
RawApiFrame::RawApiFrame(const RawApiFrame& frame) : m_data(frame.m_data) {
	
	
}
	
RawApiFrame RawApiFrame::readFrame(XBeeSerialConn& conn) throw(Error) {
	
	//If we are using unescaped data, put some limit on the frame size
	//If we find CH_DELIMITER in the data and treat is as the delimiter
	//and if the next two bytes specify a big frame length, we might get
	//stuck reading a long and invalid package.
	unsigned hiLim = 2;
	if(conn.getFlags() & XBeeSerialConn::FLAGS::ESCAPED) {
		hiLim = 0xFF;
	}
	
	
	if(conn.read() != CH_DELIMITER || conn.peek() > hiLim) {
		unsigned discarded = 1;
		
		while(conn.read() != CH_DELIMITER || conn.peek() > hiLim) {
			discarded++;
			std::clog << "D";
		}

		std::clog << std::endl;
		std::clog << "RawApiFrame: Discarded " << discarded << " byte(s)" << std::endl;
	}

	byte_vec length = conn.read(2);
	byte_vec::const_iterator iter = length.begin();
	unsigned short len = read16u(iter); //length[0] * 0x100 + length[1];
	
	if(len > 0xFF) {
		std::stringstream ss;
		ss << "Abnormal package length" << len;
		throw Error(ss.str());
	}
	
	byte_vec data = conn.read(len);
	
	byte checksum = conn.read();
	
	//Calculate checksum
	for(byte_vec::const_iterator i = data.begin(); i != data.end(); i++) {
		checksum += *i;
	}
	
	if(checksum != 0xFF) {
		throw Error("Invalid checksum");
	}
	
	if(conn.getFlags() & XBeeSerialConn::FLAGS::ESCAPED) {
		data = unescape(data);
	}
	
	return RawApiFrame(data);
}

size_t RawApiFrame::size() const {
	return m_data.size();
}
 
byte RawApiFrame::operator[] (unsigned idx) const {
	return m_data[idx];
}

std::string RawApiFrame::toString() const {
	std::stringstream ss;
	for(iterator i = this->begin(); i != this->end(); ++i) {
		ss << hex_str(*i) << " ";
	}
	return ss.str();
}

RawApiFrame::iterator RawApiFrame::begin() const {
	return m_data.begin();
}

RawApiFrame::iterator RawApiFrame::end() const {
	return m_data.end();
}

