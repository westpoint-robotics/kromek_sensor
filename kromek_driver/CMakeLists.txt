cmake_minimum_required(VERSION 3.0.2)
project(kromek_driver)

find_package(catkin REQUIRED COMPONENTS
)

###################################
## catkin specific configuration ##
###################################
## The catkin_package macro generates cmake config files for your package
## Declare things to be passed to dependent projects
## INCLUDE_DIRS: uncomment this if your package contains header files
## LIBRARIES: libraries you create in this project that dependent projects also need
## CATKIN_DEPENDS: catkin_packages dependent projects also need
## DEPENDS: system dependencies of this project that dependent projects also need
catkin_package(
  INCLUDE_DIRS include heatshrink/include
  LIBRARIES kromek_driver
#  CATKIN_DEPENDS librt libudev
#  DEPENDS system_lib
)

#########################################################################################
# Files
#########################################################################################
set (SOURCE_FILES src/CriticalSection.cpp 
					src/D3DataProcessor.cpp 
					src/DeviceBase.cpp 
					src/DeviceMgr.cpp 
					src/Event.cpp 
					src/GR1.cpp 
					src/IntervalCountProcessor.cpp 
					src/K102.cpp 
					src/Lock.cpp 
					src/RadAngel.cpp 
					src/RollingQueue.cpp 
					src/SIGMA_25.cpp 
					src/SIGMA_50.cpp 
					src/stdafx.cpp 
					src/Thread.cpp 
					src/TN15.cpp 
					src/GR05.cpp 
					src/UNIBASE.cpp 
					src/DoseDevice.cpp
					src/PacketStreamers.cpp)
					
set (HEATSHRINK_SRC heatshrink/Heatshrink.cpp
					heatshrink/heatshrink_decoder.c)

if (UNIX)
	set (SOURCE_FILES ${SOURCE_FILES} 
					src/DeviceEnumeratorLinux.cpp 
					src/USBKromekDataInterfaceLinux.cpp)
else()
	set (SOURCE_FILES ${SOURCE_FILES} 
					src/DeviceEnumeratorWindows.cpp 
					src/HIDDataInterfaceWindows.cpp 
					src/USBSerialDataInterfaceWindows.cpp)
endif()

set (HEADER_FILES 
					include/CriticalSection.h 
					include/D3DataProcessor.h 
					include/D3Structs.h 
					include/DeviceBase.h 
					include/DeviceMgr.h 
					include/Event.h 
					include/GR1.h 
					include/IDataInterface.h 
					include/IDataProcessor.h 
					include/IDevice.h 
					include/IntervalCountProcessor.h 
					include/K102.h 
					include/kromek.h 
					include/Lock.h 
					include/RadAngel.h  
					include/RollingQueue.h 
					include/SIGMA_25.h 
					include/SIGMA_50.h 
					include/stdafx.h 
					include/targetver.h 
					include/Thread.h 
					include/TN15.h 
					include/GR05.h 
					include/types.h 
					include/kromek_endian.h 
					include/UNIBASE.h
					include/DoseDevice.h
					include/PacketStreamers.h
					include/crc.h
					)
					
set (HEATSHRINK_HED	heatshrink/include/heatshrink_common.h
					heatshrink/include/heatshrink_config.h
					heatshrink/include/heatshrink_decoder.h
					heatshrink/include/Heatshrink.hpp
					)

if (WIN32)
# Include other dlls that need copying into the directory here
    #set (OTHER_RUNTIME_FILES <filePaths>)
else()
    #set (OTHER_RUNTIME_FILES <filePaths>)
endif()

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories(
# include
  ${catkin_INCLUDE_DIRS}
)

include_directories(include heatshrink/include)

if (UNIX)
	find_path (LIBUDEV_H_PATH "libudev.h")
	find_library (UDEV_LIB_PATH udev)
	find_library (RT_LIB_PATH rt)
	include_directories(${LIBUDEV_H_PATH})
	# Required for use in shared lib
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
endif()

add_definitions (-DKROMEKDRIVER_EXPORTS -D_UNICODE -DUNICODE)
add_library (${PROJECT_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES} ${HEATSHRINK_SRC} ${HEATSHRINK_HED})
target_link_libraries(${PROJECT_NAME} ${UDEV_LIB_PATH} ${RT_LIB_PATH} pthread)
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

## Add cmake target dependencies of the library
## as an example, code may need to be generated before libraries
## either from message generation or dynamic reconfigure
# add_dependencies(${PROJECT_NAME} ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})


#############
## Install ##
#############

# all install targets should use catkin DESTINATION variables
# See http://ros.org/doc/api/catkin/html/adv_user_guide/variables.html

## Mark libraries for installation
## See http://docs.ros.org/melodic/api/catkin/html/howto/format1/building_libraries.html
install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_GLOBAL_BIN_DESTINATION}
)
message(${CATKIN_PACKAGE_INCLUDE_DESTINATION})

## Mark cpp header files for installation
install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h*"
  PATTERN ".svn" EXCLUDE
)


#############
## Testing ##
#############

## Add gtest based cpp test target and link libraries
# catkin_add_gtest(${PROJECT_NAME}-test test/test_kromek_driver.cpp)
# if(TARGET ${PROJECT_NAME}-test)
#   target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME})
# endif()

## Add folders to be run by python nosetests
# catkin_add_nosetests(test)
