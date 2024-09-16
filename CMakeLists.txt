cmake_minimum_required(VERSION 3.7)

project(
		StreamerWebRtc
		VERSION 1.0)

if(POLICY CMP0079)
	cmake_policy(SET CMP0079 NEW)
endif()

set(STREAMER_SOURCES
	main.cpp
	dispatchqueue.cpp
	dispatchqueue.hpp
	h264fileparser.cpp
	h264fileparser.hpp
	helpers.cpp
	helpers.hpp
	opusfileparser.cpp
	opusfileparser.hpp
	fileparser.cpp
	fileparser.hpp
	stream.cpp
	stream.hpp
	ArgParser.cpp
	ArgParser.hpp
        h264rtspparser.h
        h264rtspparser.cpp
        opusrtspparser.h
        opusrtspparser.cpp
        rtspparser.h
        rtspparser.cpp
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTLIBS REQUIRED
    gobject-2.0
    glib-2.0
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-rtsp-1.0
    gstreamer-webrtc-1.0
    gstreamer-sdp-1.0
    gstreamer-pbutils-1.0
    json-glib-1.0)

include_directories(
		${GSTLIBS_INCLUDE_DIRS} "libs/json/include" "libs/datachannel/include")
link_directories(${GSTLIBS_LIBRARY_DIRS} "libs/datachannel")

add_executable(streamer ${STREAMER_SOURCES})

set_target_properties(streamer PROPERTIES
	CXX_STANDARD 17
	OUTPUT_NAME streamer)

set_target_properties(streamer PROPERTIES
	XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.github.paullouisageneau.libdatachannel.examples.streamer)

#find_package(Threads REQUIRED)
target_link_libraries(streamer  ${GSTLIBS_LIBRARIES} datachannel)
install(TARGETS streamer DESTINATION bin)

#lavformat \
#-lavcodec \
#-lavutil \
#-lavdevice \
#-lswscale \
#-lswresample
