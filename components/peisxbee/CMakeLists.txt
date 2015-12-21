cmake_minimum_required(VERSION 2.4)
if(COMMAND cmake_policy)
      cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

set(CMAKE_CXX_FLAGS "-g -Wall")

PROJECT(peisxbee)

ADD_EXECUTABLE (peisxbee
  ./peisxbee.cpp ./peisxbee_xbee.cpp
)

TARGET_LINK_LIBRARIES(peisxbee xbee)
TARGET_LINK_LIBRARIES(peisxbee peiskernel peiskernel_mt pthread)

INSTALL(
  TARGETS peisxbee 
  RUNTIME DESTINATION /usr/local/bin
  LIBRARY DESTINATION /usr/local/lib
  ARCHIVE DESTINATION /usr/local/lib
)
