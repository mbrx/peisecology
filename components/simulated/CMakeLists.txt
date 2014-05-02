cmake_minimum_required(VERSION
 2.4)

PROJECT(simulated)

ADD_EXECUTABLE (sim_generic
	sim_generic.cc
)

find_package(Threads) 
TARGET_LINK_LIBRARIES(sim_generic peiskernel_mt ${CMAKE_THREAD_LIBS_INIT})

INSTALL(TARGETS sim_generic
  RUNTIME DESTINATION /usr/local/bin
  LIBRARY DESTINATION /usr/local/lib
  ARCHIVE DESTINATION /usr/local/lib
)

