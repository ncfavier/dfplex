get_directory_property(isPlugin PARENT_DIRECTORY)

project(dfplex)

if (NOT isPlugin)
	cmake_minimum_required(VERSION 2.8)
	set(DFHACK_VER 0.34.11)
	set(DFHACK_REL r5)
	add_definitions(-DDFHACK_VERSION="${DFHACK_VER}-${DFHACK_REL}")
	# dfhack path
	set(DH dfhack)
endif()

# A list of source files
file (GLOB SRC "*.cpp")

# A list of headers
file (GLOB HDRS "*.hpp")

set_source_files_properties(${HDRS} PROPERTIES HEADER_FILE_ONLY TRUE)

# mash them together (headers are marked as headers and nothing will try to compile them)
list(APPEND SRC ${HDRS})

set(PROJECT_LIBS)

#linux
if(UNIX)
	add_definitions(
		-std=c++11
		-DLINUX_BUILD
		-D_WEBSOCKETPP_CPP11_STL_
	)
	list(APPEND PROJECT_LIBS pthread)
	set(DF_ROOT ${CMAKE_SOURCE_DIR}/df_linux)
# windows
else(UNIX)
	if(NOT MSVC10)
		message(FATAL_ERROR "Visual Studio 2010 required.")
	endif()
	set(WFDIR ${CMAKE_CURRENT_BINARY_DIR})
	add_definitions(
		-D_WEBSOCKETPP_CPP11_FUNCTIONAL_
		-D_WEBSOCKETPP_CPP11_SYSTEM_ERROR_
		-D_WEBSOCKETPP_CPP11_RANDOM_DEVICE_
		-D_WEBSOCKETPP_CPP11_MEMORY_
		-DNOMINMAX
		-DWIN32_LEAN_AND_MEAN
	)
	option(DL_DEPS "Download dependencies as windows binaries over https." ON)
	# Download deps
	if(DL_DEPS)
		message("Downloading dependencies. This may take a while.")
		file(DOWNLOAD https://s3.amazonaws.com/webfort/webfort-deps-r1.tar.gz
			${WFDIR}/webfort-deps.tar.gz
			EXPECTED_MD5 24e047672dc7cd787556810620838549)
		execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${WFDIR}/webfort-deps.tar.gz WORKING_DIRECTORY ${WFDIR})
		message("Dependencies downloaded :)")
	endif()

	set(SDL_DIR ${WFDIR}/SDL)
	set(CMAKE_PREFIX_PATH ${SDL_DIR})
	set(BOOST_ROOT ${WFDIR}/boost)
	link_directories(${BOOST_ROOT}/lib) # Boost doesn't find everything
endif(UNIX)

# SDL Headers
find_package(SDL 1.2 REQUIRED)
include_directories(${SDL_INCLUDE_DIRS})

# Boost/Asio
set(Boost_FIND_REQUIRED TRUE)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_SHARED_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(WS_BOOST_LIBS system)
find_package(Boost 1.38.0 REQUIRED COMPONENTS "${WS_BOOST_LIBS}")

if(Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
	list(APPEND PROJECT_LIBS ${Boost_LIBRARIES})
endif()

# websocketpp
include_directories(websocketpp)

# mongoose
include_directories(mongoose-cpp)
add_subdirectory(mongoose-cpp)

if (isPlugin)
	DFHACK_PLUGIN(dfplex ${SRC} LINK_LIBRARIES SDL dfhack-tinythread mongoose ${PROJECT_LIBS})
	target_link_libraries(dfplex ${Boost_LIBRARIES} SDL)
	INSTALL(DIRECTORY ../dist/shared/ DESTINATION .)
	INSTALL(DIRECTORY ../static/ DESTINATION ./hack/www)
else()
	if (NOT UNIX)
		message(FATAL_ERROR "Building dfplex out-of-tree not supported on windows.")
	endif()
	find_library(DFHACK_LIBRARY dfhack ${DH}/build/library)
	find_library(TINYTHREAD_LIBRARY dfhack-tinythread ${DH}/build/depends/tthread)
	list(APPEND PROJECT_LIBS
		${DFHACK_LIBRARY}
		${TINYTHREAD_LIBRARY}
	)

	include_directories(
		${DH}/library/include
		${DH}/library/proto
		${DH}/depends/protobuf
		${DH}/depends/tthread
	)

	add_library(dfplex.plug SHARED ${SRC})
	target_link_libraries(dfplex.plug mongoose ${PROJECT_LIBS})
	# Installing
	install(TARGETS dfplex.plug
		RUNTIME DESTINATION ${DF_ROOT}/hack/plugins
		LIBRARY DESTINATION ${DF_ROOT}/hack/plugins
	)
endif()
