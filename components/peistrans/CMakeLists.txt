cmake_minimum_required(VERSION 2.4)
if(COMMAND cmake_policy)
      cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

set(CMAKE_CXX_FLAGS "-g -Wall")

PROJECT(peistrans)

AUX_SOURCE_DIRECTORY(muparserx/parser MUPARSERX_FILES)

ADD_EXECUTABLE (peistrans
	./main.cpp
	./regex.cpp
	./evaluate.cpp
	./tinyxml2/tinyxml2.cpp
	./trex/trex.c
	${MUPARSERX_FILES}
)

TARGET_LINK_LIBRARIES(peistrans peiskernel peiskernel_mt pthread)

INSTALL(
	TARGETS peistrans 
	RUNTIME DESTINATION /usr/local/bin
	LIBRARY DESTINATION /usr/local/lib
  	ARCHIVE DESTINATION /usr/local/lib
)
