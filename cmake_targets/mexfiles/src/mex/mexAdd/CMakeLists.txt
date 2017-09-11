# compile mex
set(CPP_FILE mexAdd)

# set up matlab libraries
set(CPP_FILE_NAME ${CPP_FILE}.cpp)
include_directories(${MATLAB_INCLUDE_DIR})
#INCLUDE_DIRECTORIES(${MATLAB_INCLUDE_DIR} "/home/sebastian/Work/openairinterface5g/openair1/PHY/LTE_TRANSPORT/")
#GET_DIRECTORY_PROPERTY(output INCLUDE_DIRECTORIES)
#MESSAGE(WARNING ${output})

add_library(${CPP_FILE} SHARED ${CPP_FILE_NAME} ${CMAKE_SOURCE_DIR}/src/Matlabdef.def)

#include_directories("${OPENAIR_BIN_DIR}")
#include_directories("${OPENAIR1_DIR}")
#target_link_libraries(${CPP_FILE} ${MATLAB_LIBRARIES})
#target_include_directories(${CPP_FILE} PUBLIC "${OPENAIR1_DIR}")
  
# 32-bit or 64-bit mex
if(WIN32)
  if (CMAKE_CL_64)
      SET_TARGET_PROPERTIES(${CPP_FILE} PROPERTIES SUFFIX .mexw64)
  else(CMAKE_CL_64)
      SET_TARGET_PROPERTIES(${CPP_FILE} PROPERTIES SUFFIX .mexw32)
  endif(CMAKE_CL_64)
else(WIN32)
  if (CMAKE_SIZEOF_VOID_P MATCHES "8")
      SET_TARGET_PROPERTIES(${CPP_FILE} PROPERTIES SUFFIX .mexa64 PREFIX "")
  else(CMAKE_SIZEOF_VOID_P MATCHES "8")
      SET_TARGET_PROPERTIES(${CPP_FILE} PROPERTIES SUFFIX .mexglx PREFIX "")
  endif (CMAKE_SIZEOF_VOID_P MATCHES "8")
endif(WIN32)



# install to /bin by default
#install(TARGETS ${CPP_FILE} DESTINATION ./bin)
