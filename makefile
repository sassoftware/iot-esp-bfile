###
# 
# VARIABLES
#

DFESP_HOME = /opt/sas/viya/home/SASEventStreamProcessingEngine/

#Supporting aarch64, x86_64, ppc64le
ARCH := $(shell uname -p)
MODEL_NAME := $(shell lscpu | grep -i "Model name")


# ESP 6.2, 7.1 uses GCC7.4. ESP 6.1 or older uses GCC6.2 on arm64 and GCC4.9.4 on lin64.
ifeq ($(ARCH), aarch64)
    #https://devtalk.nvidia.com/default/topic/1028179/
    #Jetson TX2/Nano has Cortex A57 processor  
    #Model name:          Cortex-A57
    #Jetson AGX Xavier has Cortex-R5 processor  
    #Model name:          ARMv8 Processor rev 0 (v8l)

    ifneq (,$(findstring Cortex-R5,$(MODEL_NAME)))
        CF := -DCS_ARM -mtune=cortex-r5  -O3 -ffast-math -flto -march=armv8-a+crypto -mcpu=cortex-r5+crypto
    endif

    ifneq (,$(findstring Cortex-A57,$(MODEL_NAME)))
        CF := -DCS_ARM -mtune=cortex-a57 -O3 -ffast-math -flto -march=armv8-a+crypto -mcpu=cortex-a57+crypto
    endif

    LF := -lpthread 
    # CXX := g++-6
    # CC := gcc-6
else
    ifneq (,$(filter $(ARCH),x86_64 ppc64le))
        CF := -m64
		LF := -lpthread -lleveldb
    #    CC := gcc-4
	# 	 CXX := g++-4
    endif
endif



# -- GCC on Linux
CXX=g++
CXXFLAGS=-g $(CF) -std=c++11 -fPIC -DUSE_BOOST -DUSE_MCT -DOS_LINUX  -Wall -D_REENTRANT -D_THREAD_SAFE -O3 -ldl 
CC=gcc
CFLAGS=-g -std=c++11 -DUSE_BOOST -DUSE_MCT $(CF)
LDFLAGS=$(LF) -L$(DFESP_HOME)/lib -L$(DFESP_HOME)/lib/tk -L/opt/sas/viya/home/SASFoundation/sasexe
SHLIBFLAGS=-shared 


# -- Preprocessor flags (includes header search path)
#    Add any additional header directories here.

CPPFLAGS=        -I. \
                 -I$(DFESP_HOME)/include/utils \
                 -I$(DFESP_HOME)/include/cxx \
                 -I$(DFESP_HOME)/include/pubsub \
                 -I$(DFESP_HOME)/include/connectors \
                 -I$(DFESP_HOME)/include/esptk \
                 -I$(DFESP_HOME)/include \
				 -I/usr/include \
				 $(POST_INCS) \
                 -I.

ifeq ($(ARCH), aarch64)
CPPFLAGS += -I/usr/include/aarch64-linux-gnu
LDFLAGS += -L/usr/lib/aarch64-linux-gnu
endif			

# -- Libraries
#    Add any additional libraries here. 
LDFLAGS +=-L/usr/lib64 \
		  -L$(DFESP_HOME)/lib \
          -L./lib \
		  -L./plugins \
		  -L/usr/lib -L/usr/local/lib 

	  

LIBS= -ldfxesp_connectors -ldfxesp_pubsub -ldfxesp -lesptk -ldfxesp_utils -lboost_system


# -- Outpur library name
#    This file will be created by a successful make.
LIBNAME=plugins/$(ESP_VERSION)/$(ARCH)/libesp_bfile_cpi.so

# -- Object files
#    There should be one .o for each .cpp file.
OBJ  := $(patsubst %.c,%.o,$(wildcard src/*.c)) $(patsubst %.cpp,%.o,$(wildcard src/*.cpp))

#
# End of variables section. 
# You should not need to change anything below here.
#

###
#
# RULES
#

all: echo_env plugindir lib

plugindir:
	mkdir -p plugins
	mkdir -p plugins/$(ESP_VERSION)
	mkdir -p plugins/$(ESP_VERSION)/$(ARCH)

echo_env:
	@echo 
	@echo current environment:
	@echo DFESP_HOME=$$DFESP_HOME
	@echo CXX=$(CXX)
	@echo CXXFLAGS=$(CXXFLAGS)
	@echo CPPFLAGS=$(CPPFLAGS)
	@echo LDFLAGS=$(LDFLAGS)
	@echo [ if you see errors during compilation you may need to change these values. ]
	@echo 

clean:
	rm -fr $(OBJ) $(EXECNAME) $(LIBNAME)

exec: $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(EXECNAME) $(OBJ) $(LIBS)

lib: $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SHLIBFLAGS) -o $(LIBNAME) $(OBJ) $(LIBS) $(EXTRA_LDFLAGS)

src/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $<

src/%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<


