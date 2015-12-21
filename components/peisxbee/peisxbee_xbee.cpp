#include "libxbee/libXBee.h"

extern "C" {
#include <peiskernel/peiskernel.h>
#include <peiskernel/peiskernel_mt.h>
}

#include <iostream>
#include <string>
#include <sstream>
#include <ctime>

#include <unistd.h>

#include <algorithm>

extern volatile bool doRun;

template <typename T>
void insertTuple(const std::string& name, const T& val) {
	
	std::stringstream ss;
	ss << val;
	std::string value = ss.str();

	peiskmt_lock();
	
	PeisTuple tuple;
	peisk_initTuple(&tuple);
	peisk_gettime2(&tuple.ts_expire[0], &tuple.ts_expire[1]);
	tuple.owner = peisk_peisid();
	peisk_setTupleName(&tuple, const_cast<char*>(name.c_str()));
	tuple.data = const_cast<char*>(value.c_str());
	tuple.datalen = value.size() + 1;
	
	//Calculate the tuple life time dynamically so that tuples that are often updated
	//expires quicker than those that are updated seldomly.
	//This will help us spot connection problems more easily.
	unsigned lifeSpan = 60*5; //Default 5 minutes
	{
		PeisTuple* oldTuple = peisk_getTuple(peisk_peisid(), name.c_str(), PEISK_NON_BLOCKING | PEISK_ENCODING_ASCII);
		if(oldTuple != NULL) {
			unsigned delta = std::max(1, tuple.ts_expire[0] - oldTuple->ts_write[0]);
			//Set the expiry time to 4 times the frequency, minimum 4 seconds
			lifeSpan = delta * 4;
		}
	}
	
	tuple.ts_expire[0] = peisk_gettime() + lifeSpan; //Set the expiration time
	tuple.ts_expire[1] = 0;
	
	peisk_insertTuple(&tuple);
	
	peiskmt_unlock();
}

void* xbeeThreadFcn(void* ptr) {
	
	XBee* xbee = reinterpret_cast<XBee*>(ptr);
	
	while(doRun && peiskmt_isRunning()) {
		try {
			RawApiFrame rawframe = xbee->readFrame();
			
			if(rawframe[0] == AdcDioFrame::AD_API_FRAME_ID) {
				
				AdcDioFrame frame(rawframe);
				
				std::stringstream infoStringStream;
				
				//Print some log output
				infoStringStream << std::time(0) << ": " <<  "Got " << frame.size() << " sample(s) from XBee " << hex_str(frame.getSourceAdress()) << std::endl;
				
				infoStringStream << frame.toString() << std::endl;
				
				for(unsigned s = 0; s < frame.size(); ++s) {
					infoStringStream << "Sample " << s << ":" << std::endl;
					infoStringStream << frame[s].dioSample.toString() << std::endl;
					infoStringStream << frame[s].adSample.toString() << std::endl;
					infoStringStream << std::endl << std::endl;
				}
				
				//Print some more log output
				std::clog << infoStringStream.str();
				
				std::string tupleBaseName;
				{
					std::stringstream ss;
					ss << "xbees." << std::hex << frame.getSourceAdress();
					tupleBaseName = ss.str();
				}
				
				insertTuple(tupleBaseName.c_str(), infoStringStream.str());
				
				if(frame.size() > 0) {
					//The contents chould be the same in each sample
					const AdSample& adSampleZero = frame[0].adSample;
					for(unsigned ch = 0; ch < AdcDioFrame::NUM_ADS; ++ch) {
						
						if(adSampleZero.isSet(ch)) {
							std::stringstream dataStream;
							std::stringstream nameStream;
							nameStream << tupleBaseName << ".ad" << ch;
							
							for(unsigned s = 0; s < frame.size(); ++s) {
								dataStream << (dataStream.str().size() == 0 ? "" : " ") << frame[s].adSample.getChannel(ch);
							}
							
							insertTuple(nameStream.str(), dataStream.str());
						}
					}
					
					const DioSample& dioSampleZero = frame[0].dioSample;
					for(unsigned ch = 0; ch < AdcDioFrame::NUM_DIOS; ++ch) {
						
						if(dioSampleZero.isSet(ch)) {
							std::stringstream dataStream;
							std::stringstream nameStream;
							nameStream << tupleBaseName << ".dio." << ch;
							
							for(unsigned s = 0; s < frame.size(); ++s) {
								dataStream << (dataStream.str().size() == 0 ? "" : " ") << frame[s].dioSample.getChannel(ch);
							}
							
							insertTuple(nameStream.str(), dataStream.str());
						}
					}
					
				} else {
					std::cerr << "Empty sample recevied: " << frame.toString() << ", this is strange..." << std::endl;
				}
				
			} else {
				std::clog << "Unknown frame " << rawframe[0] << " received: " << rawframe.toString() << std::endl;
			}
			
		} catch(Error e) {
			std::cerr << "Caught error: " << e << std::endl;
			usleep(10);
		}
	}
	
	return NULL;
}

