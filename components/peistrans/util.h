#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <list>

std::vector<std::string>& splitstr(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> splitstr(const std::string &s, char delim) {
    std::vector<std::string> elems;
    return splitstr(s, delim, elems);
}

//Create a string out of a a series of elements and delimit them with delim
template <typename iter>
std::string concatstr(iter begin, iter end, char delim) {
	
	std::stringstream ss;
	
	while(begin != end) {
		ss << *begin;
		if(++begin != end) {
			ss << delim;
		}
	}
	
	return ss.str();
}

//Create a string out of a a series of elements and delimit them with delim but ignore certaing elements
template <typename iter, typename type>
std::string concatstrI(iter begin, iter end, char delim, type ignore) {
	
	std::vector<type> cpy(begin, end);
	
	for(typename std::vector<type>::iterator i = cpy.begin(); i != cpy.end(); ) {
		if(*i == ignore) {
			i = cpy.erase(i);
		} else {
			++i;
		}
	}
	
	return concatstr(cpy.begin(), cpy.end(), delim);
}

