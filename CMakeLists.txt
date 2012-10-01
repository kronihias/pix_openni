cmake_minimum_required (VERSION 2.6) 

SET(CMAKE_BUILD_TYPE "Release")

SET( PD_INC C:/Users/GVITC/Documents/_Matthias/pd-bin/src ) 
SET( PD_LIB C:/Users/GVITC/Documents/_Matthias/pd-bin/bin ) 

SET( GEM_INC C:/Users/GVITC/Documents/_Matthias/Gem-0.93.3-bin/Gem/dev ) 
SET( GEM_LIB C:/Users/GVITC/Documents/_Matthias/Gem-0.93.3-bin/Gem/dev ) 

SET( OPENNI_INC C:/Users/GVITC/Documents/_Matthias/OpenNI/Include ) 
SET( OPENNI_LIB C:/Users/GVITC/Documents/_Matthias/OpenNI/Lib ) 

INCLUDE_DIRECTORIES( . .. ${PD_INC} ${GEM_INC} ${OPENNI_INC} )
LINK_DIRECTORIES( ${PD_LIB} ${GEM_LIB} ${OPENNI_LIB} )

add_library (pix_openni SHARED pix_openni.cc) 

target_link_libraries (pix_openni Gem pd openNI)