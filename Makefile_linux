# change to your local directories!

#PD_DIR = /Applications/Pd.app/Contents/Resources
#GEM_DIR = /Users/matthias/Gem

PD_DIR = /home/matthias/pd
GEM_DIR = /home/matthias/Gem
OPEN_NI_DIR = /home/matthias/OpenNI-Bin-Dev-Linux-x86-v1.3.4.6

# build flags

INCLUDES =  -I$(PD_DIR)/src -I. -I$(OPEN_NI_DIR)/Include -I$(GEM_DIR)/src -I$(PD_DIR)/src
CPPFLAGS  = -fPIC -DPD -O2 -funroll-loops -fomit-frame-pointer  -ffast-math \
    -Wall -W -Wno-unused -Wno-parentheses -Wno-switch -lOpenNI\
    -DGEM_OPENCV_VERSION=\"$(GEM_OPENCV_VERSION)\" -g


UNAME := $(shell uname -s)
ifeq ($(UNAME),Linux)
 CPPFLAGS += -DLINUX
 INCLUDES += ``
 LDFLAGS =  -export_dynamic -shared -lOpenNI
 LIBS = -lOpenNI
 EXTENSION = pd_linux
endif
ifeq ($(UNAME),Darwin)
 CPPFLAGS += -DDARWIN
 INCLUDES += -I
 LDFLAGS =  -arch i386 -bundle -undefined dynamic_lookup -flat_namespace 
 LIBS =  -lm -lOpenNI
 EXTENSION = pd_darwin
endif

.SUFFIXES = $(EXTENSION)

SOURCES = pix_openni.cc

all: $(SOURCES:.cc=.$(EXTENSION)) $(SOURCES_OPT:.cc=.$(EXTENSION))

%.$(EXTENSION): %.o
	gcc $(LDFLAGS) -o $*.$(EXTENSION) $*.o $(LIBS)

.cc.o:
	g++ $(CPPFLAGS) $(INCLUDES) -o $*.o -c $*.cc

.c.o:
	gcc $(CPPFLAGS) $(INCLUDES) -o $*.o -c $*.c

clean:
	rm -f pix_openni*.o
	rm -f pix_openni*.$(EXTENSION)

distro: clean all
	rm *.o
