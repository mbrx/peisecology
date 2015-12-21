cmake_minimum_required(VERSION 2.4)
if(COMMAND cmake_policy)
      cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

set(CMAKE_CXX_FLAGS "-g -Wall")

PROJECT(libxbee)

ADD_LIBRARY (xbee
  libXBee_AdcDioFrame.cpp
	libXBee_Defs.cpp
	libXBee_Error.cpp
  libXBee_RawApiFrame.cpp
	libXBee_XBee.cpp
	libXBee_XBeeSerialConn.cpp
)

FILE(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
INSTALL(FILES ${files} DESTINATION /usr/local/include/xbee)

INSTALL(
  TARGETS xbee 
  RUNTIME DESTINATION /usr/local/bin
  LIBRARY DESTINATION /usr/local/lib
  ARCHIVE DESTINATION /usr/local/lib
)
