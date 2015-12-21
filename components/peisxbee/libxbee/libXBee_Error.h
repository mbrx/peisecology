#pragma once

#include <string>
#include <iostream>

class Error {
	const std::string m_desc;
	const int m_errno;
public:
	static const int DEFAULT_ERRNO;
	Error(const Error& err);
	Error(const std::string& err, int errno = DEFAULT_ERRNO);
	
	std::string str() const;
};

std::ostream& operator << (std::ostream& ios, const Error& err);

