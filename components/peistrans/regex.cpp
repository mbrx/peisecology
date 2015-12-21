#include "regex.h"




RegExp::RegExp(const std::string& pattern) {
	Compile(pattern.c_str());
}
	
bool RegExp::Match(const std::string& text) const {
	return const_cast<TRexpp&>(static_cast<const TRexpp&>(*this)).Match(text.c_str());
}

