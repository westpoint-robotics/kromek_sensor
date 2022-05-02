cmake_minimum_required(VERSION 3.0.2)
project(spectrometer_driver)
SET(VERSION_PRODUCT 16)
SET(VERSION_MAJOR 3)
SET(VERSION_MINOR 8)
SET(VERSION_BUILD 23)

## Compile as C++11, supported in ROS Kinetic and newer
# add_compile_options(-std=c++11)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  kromek_driver
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
  INCLUDE_DIRS include
  LIBRARIES spectrometer_driver
#  CATKIN_DEPENDS kromek_driver
#  DEPENDS system_lib
)

#########################################################################################
# Files
#########################################################################################

set (HEADER_FILES 	include/Detector.h
					include/devices.h
					include/DriverMgr.h
					include/SpectrometerData.h
					include/SpectrometerDriver.h
					include/stdafx.h
					include/targetver.h)

set (SOURCE_FILES 	src/Detector.cpp 
					src/DriverMgr.cpp 
					src/SpectrometerDriver.cpp
					src/stdafx.cpp)

set (OTHER_RUNTIME_FILES )

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)
message(${catkin_INCLUDE_DIRS})

find_path (LIBUDEV_H_PATH "libudev.h")
find_library (LIBUDEV_LIB_PATH udev)
include_directories(${LIBUDEV_H_PATH})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-enum-compare")

add_definitions (-DUSBSPECTROMETERDLL_EXPORTS -D_UNICODE)
add_library (${PROJECT_NAME}Static STATIC ${SOURCE_FILES} )
add_library (${PROJECT_NAME} SHARED ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME}Static ${catkin_LIBRARIES} ${LIBUDEV_LIB_PATH})
target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES} ${LIBUDEV_LIB_PATH})
target_compile_definitions(${PROJECT_NAME}    PRIVATE VERSION_PRODUCT=${VERSION_PRODUCT}
                                            VERSION_MAJOR=${VERSION_MAJOR}
                                            VERSION_MINOR=${VERSION_MINOR}
                                            VERSION_BUILD=${VERSION_BUILD})

target_compile_definitions(${PROJECT_NAME}Static    PRIVATE VERSION_PRODUCT=${VERSION_PRODUCT}
                                            VERSION_MAJOR=${VERSION_MAJOR}
                                            VERSION_MINOR=${VERSION_MINOR}
                                            VERSION_BUILD=${VERSION_BUILD})

# -fPIC
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET ${PROJECT_NAME}Static PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION ${VERSION_MAJOR}.${VERSION_MINOR})

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

## Mark cpp header files for installation
install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h"
  PATTERN ".svn" EXCLUDE
)

#############
## Testing ##
#############

## Add gtest based cpp test target and link libraries
# catkin_add_gtest(${PROJECT_NAME}-test test/test_spectrometer_driver.cpp)
# if(TARGET ${PROJECT_NAME}-test)
#   target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME})
# endif()

## Add folders to be run by python nosetests
# catkin_add_nosetests(test)
