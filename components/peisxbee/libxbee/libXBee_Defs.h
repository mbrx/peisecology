#pragma once

#include <iostream>
#include <sstream>
#include <vector>

typedef unsigned char byte;
typedef std::vector<byte> byte_vec;

template <typename _ty>
std::string hex_str(_ty b) {
	std::stringstream ss;
	ss << "0x";
	ss.fill('0');
	ss.width(2*sizeof(_ty));
	ss << std::hex << (unsigned) b;
	return ss.str();
}

template <typename _ti>
unsigned short read16u(_ti& iterator) {
	unsigned short val = 0;
	val += 0x0100 * *iterator++;// iterator++;
	val += 0x0001 * *iterator++;// iterator++;
	return val;
}

template <typename _ti>
unsigned char read8u(_ti& iterator) {
	unsigned char val = *iterator++;// iterator++;
	return val;
}
