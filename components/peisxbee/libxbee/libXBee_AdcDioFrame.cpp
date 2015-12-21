#include "libXBee_AdcDioFrame.h"
#include "libXBee_Error.h"
#include <sstream>
#include <limits>
#include <iomanip>

#include "libXBee_Defs.h"

const byte AdcDioFrame::AD_API_FRAME_ID = 0x83;
const unsigned AdcDioFrame::NUM_ADS = 6;
const unsigned AdcDioFrame::NUM_DIOS = 9;

bool DioSample::isSet(unsigned n) const {
	return (sampleExistsFlags & (1 << n)) != 0;
}

bool DioSample::getChannel(unsigned n) const {
	if(!isSet(n)) {
		std::stringstream ss;
		ss << "DioSample::getChannel(n): " << n << " not set!";
		throw new Error(ss.str());
	}
	return (readings & (1 << n)) != 0;
}

std::string DioSample::toString() const {
	std::stringstream ss;
	
	ss << "DIO: {";
	ss << std::setiosflags(std::ios::left);
	for(unsigned i = 0; i < AdcDioFrame::NUM_DIOS; ++i) {
		ss << i << ": ";
		if(isSet(i)) {
			ss << std::setw(4);
			ss << getChannel(i);
		} else {
			ss << std::setw(4);
			ss << " ";
		}
		ss << " ";
	}
	ss << "}";
	
	return ss.str();
}


bool AdSample::isSet(unsigned n) const {
	return (sampleExistsFlags & (1 << n)) != 0;
}

unsigned short AdSample::getChannel(unsigned n) const {
	if(!isSet(n)) {
		std::stringstream ss;
		ss << "AdSample::getChannel(n): " << n << " not set!";
		throw new Error(ss.str());
	}
	
	return readings[n];
}

std::string AdSample::toString() const {
	std::stringstream ss;
	
	ss << "AD:  {";
	ss << std::setiosflags(std::ios::left);
	for(unsigned i = 0; i < AdcDioFrame::NUM_ADS; ++i) {
		ss << i << ": ";
		if(isSet(i)) {
			ss << std::setw(4);
			ss << getChannel(i);
		} else {
			ss << std::setw(4);
			ss << " ";
			
		} 
		ss << " ";
	}
	ss << "}";
	
	return ss.str();
}

AdcDioFrame::AdcDioFrame(const RawApiFrame& frame) throw(Error) {
	
	RawApiFrame::iterator iter = frame.begin();
	
	if(*iter++ != AD_API_FRAME_ID) {
		throw Error(std::string("Not an AD frame: ") +  frame.toString());
	}
	
	m_sourceAddress = read16u(iter);
	m_rssiVal = read8u(iter);
	m_options = read8u(iter);
	m_quantity = read8u(iter);
	
	if(m_quantity != 1) {
		std::cerr << "Parsing of sample quantities higher than 1 is untested, RAW:" << frame.toString() << std::endl;
	}
	
	//MSB [NA A5 A4 A3 A2 A1 A0 D8 D7 D6 D5 D4 D3 D2 D1 D0] LSB
	m_channelIndicator = read16u(iter);
	
	/*
	if((m_channelIndicator & ((1 << NUM_DIOS) - 1)) != 0) {
		std::stringstream ss;
		ss << "DIO not supported";
		ss << ", channel indicator " << hex_str(m_channelIndicator);
		ss << ", from: "             << hex_str(m_sourceAddress);
		ss << ", rssi: "             << hex_str(m_rssiVal);
 		ss << ", options: "          << hex_str(m_options);
		throw Error(ss.str());
	}
	*/
	
	for(; iter != frame.end();) {
		
		DioSample dioSample;
		dioSample.sampleExistsFlags = getDioBits();
		
		if(getDioBits() != 0) {
			dioSample.readings = read16u(iter);
		}
		
		AdSample adSample;
		adSample.sampleExistsFlags = getAdBits();
		
		for(unsigned n = 0; n < NUM_ADS; n++) {
			if(adSample.isSet(n)) {
				adSample.readings[n] = read16u(iter);
			}
		}
		
		Sample sample = {dioSample, adSample};
		
		m_samples.push_back(sample);
	}
}

unsigned AdcDioFrame::getSourceAdress() const {
	return m_sourceAddress;
}

unsigned AdcDioFrame::getRssiValue() const {
	return m_rssiVal;
}

unsigned AdcDioFrame::getOptions() const {
	return m_options;
}
	
size_t AdcDioFrame::size() const {
	return m_samples.size();
}

const Sample& AdcDioFrame::operator[](unsigned idx) const {
	return m_samples[idx];
}

unsigned short AdcDioFrame::getDioBits() const {
	return m_channelIndicator & ((1 << NUM_DIOS) - 1);
}

unsigned short AdcDioFrame::getAdBits() const {
	return ((m_channelIndicator >> NUM_DIOS) & 0xFF);
}

std::string AdcDioFrame::toString() const {
	std::stringstream ss;
	ss << "Source addr.: " << hex_str(m_sourceAddress) << " (" << m_sourceAddress << ")";
	ss << ", RSSI: " << "-" << (unsigned) m_rssiVal << "dBm";
	ss << ", Opts.: " << hex_str(m_options);
	ss << ", Quantity: " << (unsigned) m_quantity;
	ss << ", Channel ind.: " << hex_str(m_channelIndicator);
	
	return ss.str();
}
