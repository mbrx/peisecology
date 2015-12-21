#include "evaluate.h"

#include <string>
#include <sstream>

#include "./muparserx/parser/mpParser.h"

const std::string EVAL_VAR = "x";
const std::string ERROR_PREFIX_STRING = "ERROR: ";

typedef std::string ConversionError;

template <typename _ty>
_ty convert(std::string str) throw(ConversionError) {
	std::stringstream ss; ss << str;
	_ty t; ss >> t;
	
	if(ss.fail()) {
		throw ConversionError(std::string("Could not convert ") + str);
	} else {
		return t;
	}
}

std::string evaluate(const std::string& value, const std::string& expression, DataType dataType) {
	
	try {
		mup::ParserX parser(mup::pckALL_NON_COMPLEX);
		
		mup::Value xVal;
		if(dataType == DT_DOUBLE) {
			xVal = mup::Value(convert<double>(value));
		} else if(dataType == DT_STRING) {
			xVal = mup::Value(value);
		} else if(dataType == DT_INTEGER) {
			xVal = mup::Value(convert<int>(value));
		} else if(dataType == DT_BOOLEAN) {
			xVal = mup::Value(convert<bool>(value));
		} else {
			throw ConversionError("Unhandled data type");
		}
		
		parser.DefineVar(EVAL_VAR, mup::Variable(&xVal));
		parser.SetExpr(expression);
	
		mup::Value yVal = parser.Eval();
		
		if(yVal.IsString()) {
			//Return the string without quotes (as ToString() would).
			return yVal.GetString();
		} else {
			return yVal.ToString();
		}
		
	} catch (mup::ParserError err) {
		return ERROR_PREFIX_STRING + err.GetMsg();
	} catch (ConversionError err) {
		return ERROR_PREFIX_STRING + err;
	}
}

