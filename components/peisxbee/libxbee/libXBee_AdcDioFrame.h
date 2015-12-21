#pragma once
#include "libXBee_RawApiFrame.h"
#include <vector>
#include <string>

class AdcDioFrame;

class DioSample {
	friend class AdcDioFrame;
	unsigned short sampleExistsFlags;
	unsigned short readings;
public:
	bool isSet(unsigned n) const;
	bool getChannel(unsigned n) const;
	std::string toString() const;
};

class AdSample {
	friend class AdcDioFrame;
	unsigned short sampleExistsFlags;
	unsigned short readings[6];
public:
	bool isSet(unsigned n) const;
	unsigned short getChannel(unsigned n) const;
	std::string toString() const;
};

struct Sample {
	DioSample dioSample;
	AdSample adSample;
};

//This class is used to read ADC and DIO samples from an XBee.
//It is important to note that one sample is not a single reading.
//Rather, one sample is a the state of all ADC and DIO ports at a 
//particular time.
class AdcDioFrame {
	unsigned short m_sourceAddress;
	byte m_rssiVal;
	byte m_options;
	byte m_quantity;
	unsigned short m_channelIndicator;
	std::vector<Sample> m_samples;
public:
	static const byte AD_API_FRAME_ID;
	static const unsigned NUM_ADS;
	static const unsigned NUM_DIOS;
	
	AdcDioFrame(const RawApiFrame& frame) throw(Error);
	
	unsigned getSourceAdress() const;
	unsigned getRssiValue() const;
	unsigned getOptions() const;
	
	size_t size() const;
	const Sample& operator[](unsigned idx) const;
	
	std::string toString() const;
private:
	unsigned short getDioBits() const;
	unsigned short getAdBits() const;
};
