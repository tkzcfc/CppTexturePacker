project(libplist)


file(GLOB_RECURSE LIB_PLIST_HPP 
	src/*.h)
	
file(GLOB_RECURSE LIB_PLIST_C 
	src/*.c)
	
file(GLOB_RECURSE LIB_PLIST_CPP
	src/*.cpp)


# STATIC SHARED
add_library(${PROJECT_NAME} STATIC
    ${LIB_PLIST_HPP}
	${LIB_PLIST_C}
	${LIB_PLIST_CPP}
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)


