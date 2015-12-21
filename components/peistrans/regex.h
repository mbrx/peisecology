#pragma once

#include <string>

#include "./trex/TRexpp.h"

//This class is just adding some functionality to the TRexpp class (which is horrible)
class RegExp : protected TRexpp {
public:
	RegExp(const std::string& pattern);
	bool Match(const std::string& text) const;
};

