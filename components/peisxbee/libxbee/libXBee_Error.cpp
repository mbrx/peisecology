#include "libXBee_Error.h"

#include <cstring>

const int Error::DEFAULT_ERRNO = 0;

Error::Error(const Error& err) : m_desc(err.m_desc), m_errno(err.m_errno) {
	
}

Error::Error(const std::string& err, int errno) :
m_desc(err), m_errno(errno)
{
	
}

std::string Error::str() const {
	
	std::string desc = m_desc;
	
	
	if(m_errno != DEFAULT_ERRNO) {
		desc = desc + " (" + strerror(m_errno) + ")";
	}
	
	return desc;
}



std::ostream& operator << (std::ostream& ios, const Error& err) {
	return ios << err.str();
}

