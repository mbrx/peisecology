#pragma once

#include <string>



enum DataType {
	DT_DOUBLE,
	DT_STRING, 
	DT_INTEGER, 
	DT_BOOLEAN
	//DT_COMPLEX,
	//DT_ARRAY
};

std::string evaluate(const std::string& value, const std::string& expression, DataType dataType = DT_DOUBLE);