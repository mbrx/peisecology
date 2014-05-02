cmake_minimum_required(VERSION 2.4)

PROJECT(tuplemonitor)

ADD_EXECUTABLE (tuplemonitor
	tuplemonitor.cc
)

find_package(Threads)
TARGET_LINK_LIBRARIES(tuplemonitor peiskernel_mt ${CMAKE_THREAD_LIBS_INIT})

INSTALL(TARGETS tuplemonitor
  RUNTIME DESTINATION /usr/local/bin
  LIBRARY DESTINATION /usr/local/lib
  ARCHIVE DESTINATION /usr/local/lib
)

