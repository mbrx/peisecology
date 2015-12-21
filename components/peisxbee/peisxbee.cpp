#include <iostream>
#include <string>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>

#include "libxbee/libXBee.h"

#ifdef __cplusplus
extern "C"
{
#endif	
	#include <peiskernel/peiskernel.h>
	#include <peiskernel/peiskernel_mt.h>
#ifdef __cplusplus
}
#endif

volatile bool doRun = true;
const char TUPLE_DO_QUIT[] = "do-quit";

void* xbeeThreadFcn(void* ptr);

int main(int argc, char **args) {
	
	std::cout << "Version 0.0.2" << std::endl;
	
	std::string port = "/dev/ttyUSB0";
	unsigned baudRate = 9600;
	
	for(int i = 1; i < argc; i++) {
		std::string arg = args[i];
		
		if(arg == "--help") {
			std::cout << "Arguments: --device [device(" << port << ")] --baudrate [baudrate(" << baudRate << ")] {peis arguments}" << std::endl;
			peisk_printUsage(stdout, argc, args);
			return EXIT_SUCCESS;
		} else if(arg == "--device") {
			port = args[++i];
		} else if(arg == "--baudrate") {
			//baudRate = args[++i];
			std::stringstream ss;
			ss << args[++i];
			ss >> baudRate;
			if(ss.bad()) {
				std::cout << "Invalid baud rate" << std::endl;
				return 1;
			}
		}
	}
	
	std::cout << "Using " << port << " at " << baudRate << " baud" << std::endl;
	
	try {
		XBee xbee(port.c_str(), baudRate);
		
		peiskmt_initialize(&argc,args);
		
		static pthread_t xbeeThread;
		
		pthread_create(&xbeeThread, NULL, xbeeThreadFcn, &xbee);
		
		//Handle SIGINT by closing the listening socket
		struct LocalFunctionWrapper {
			static void siginthandler(int param) {
				std::clog << "Got SIGINT, closing down" << std::endl;
				peiskmt_shutdown();
				//Shut down the xbee thread!.
				exit(EXIT_SUCCESS);
			}
		};
		//Register the signal handler
		signal(SIGINT, LocalFunctionWrapper::siginthandler);
		
		peiskmt_setStringTuple(TUPLE_DO_QUIT, "");
		
		while(doRun && peiskmt_isRunning()) {
			
			peiskmt_lock();
			PeisTuple* tuple = peisk_getTuple(peisk_peisid(), TUPLE_DO_QUIT, PEISK_NON_BLOCKING | PEISK_ENCODING_ASCII);
			
			if(tuple != 0 && tuple->datalen > 1) {
				doRun = false;
			}
			peiskmt_unlock();
			
			usleep(10);
		}
		
		//This won't join until the Xbee reads a package
		pthread_join(xbeeThread, NULL);
		
		peiskmt_shutdown();
	} catch(Error err) {
		std::cout << "Error: " << err << std::endl;
	}
	
	return 0;
}


