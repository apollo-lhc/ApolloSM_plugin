SHELL = bash

defaut:build
include mk/version.mk


BUTOOL_PATH?=../../

UHAL_VER_MAJOR ?= 2
UHAL_VER_MINOR ?= 8


CXX?=g++

IPBUS_REG_HELPER_PATH?=${BUTOOL_PATH}/plugins/BUTool-IPBUS-Helpers

LIBRARY_APOLLO_SM_DEVICE = lib/libBUTool_ApolloSMDevice.so
#make sure version.cc isn't found here in the wildcard, otherwise it might get added twice. 
LIBRARY_APOLLO_SM_DEVICE_SOURCES = $(patsubst $(VERSION_FILE),,$(wildcard src/ApolloSM_device/*.cc))
#add version.cc
LIBRARY_APOLLO_SM_DEVICE_SOURCES += $(VERSION_FILE)
LIBRARY_APOLLO_SM_DEVICE_OBJECT_FILES = $(patsubst src/%.cc,obj/%.o,${LIBRARY_APOLLO_SM_DEVICE_SOURCES})

LIBRARY_APOLLO_SM = lib/libBUTool_ApolloSM.so
LIBRARY_APOLLO_SM_SOURCES = $(wildcard src/ApolloSM/*.cc)
LIBRARY_APOLLO_SM_OBJECT_FILES = $(patsubst src/%.cc,obj/%.o,${LIBRARY_APOLLO_SM_SOURCES})

EXE_APOLLO_SM_STANDALONE = bin/
EXE_APOLLO_SM_STANDALONE_SOURCE = $(wildcard src/standalone/*.cxx)
EXE_APOLLO_SM_STANDALONE_BIN = $(patsubst src/standalone/%.cxx,bin/%,${EXE_APOLLO_SM_STANDALONE_SOURCE})
EXE_APOLLO_SM_STANDALONE_SOURCES = $(wildcard src/standalone/*.cc)
EXE_APOLLO_SM_STANDALONE_OBJECT_FILES += $(patsubst src/%.cc,obj/%.o,${EXE_APOLLO_SM_STANDALONE_SOURCES})

# Python binding related
SOURCE_APOLLO_SM_PYBIND = python/ApolloSM_PyBind.cpp 
LIBRARY_APOLLO_SM_PYBIND = lib/ApolloSM$(shell python3-config --extension-suffix)
PYBIND11_PATH=pybind11
# The directory where the Python3 ApolloSM library will be copied to
PYTHON3_INSTALL_PATH ?= $(shell python3 -c "from distutils.sysconfig import get_python_lib; print(get_python_lib())")

INCLUDE_PATH += \
							-Iinclude  \
							-I$(BUTOOL_PATH)/include \
							-I$(IPBUS_REG_HELPER_PATH)/include

LIBRARY_PATH += \
							-Llib \
							-L$(BUTOOL_PATH)/lib \
							-L$(IPBUS_REG_HELPER_PATH)/lib

ifdef BOOST_INC
INCLUDE_PATH +=-I$(BOOST_INC)
endif
ifdef BOOST_LIB
LIBRARY_PATH +=-L$(BOOST_LIB)
endif

LIBRARIES =    	-lcurses \
		-lToolException	\
		-lBUTool_IPBusIO \
		-lBUTool_IPBusRegHelpers \
		-lBUTool_IPBusStatus \
		-lBUTool_BUTextIO \
		-lboost_regex \
		-lboost_filesystem \
		-lboost_program_options

INSTALL_PATH ?= ./install


CXX_FLAGS = -std=c++11 -g -O3 -rdynamic -Wall -MMD -MP -fPIC ${INCLUDE_PATH} -Werror -Wno-literal-suffix

CXX_FLAGS +=-fno-omit-frame-pointer -Wno-ignored-qualifiers -Werror=return-type -Wextra -Wno-long-long -Winit-self -Wno-unused-local-typedefs  -Woverloaded-virtual -DUHAL_VER_MAJOR=${UHAL_VER_MAJOR} -DUHAL_VER_MINOR=${UHAL_VER_MINOR} ${COMPILETIME_ROOT} ${FALLTHROUGH_FLAGS}

ifdef MAP_TYPE
CXX_FLAGS += ${MAP_TYPE}
endif

LINK_LIBRARY_FLAGS = -shared -fPIC -Wall -g -O3 -rdynamic ${LIBRARY_PATH} ${LIBRARIES} \
			-Wl,-rpath=$(RUNTIME_LDPATH)/lib ${COMPILETIME_ROOT}

LINK_EXE_FLAGS     = -Wall -g -O3 -rdynamic ${LIBRARY_PATH} ${LIBRARIES} \
			-lBUTool_Helpers \
			-Wl,-rpath=$(RUNTIME_LDPATH)/lib ${COMPILETIME_ROOT} 



# ------------------------
# IPBUS stuff
# ------------------------
UHAL_LIBRARIES = -lcactus_uhal_log 		\
                 -lcactus_uhal_grammars 	\
                 -lcactus_uhal_uhal 		

# Search uHAL library from $IPBUS_PATH first then from $CACTUS_ROOT
ifdef IPBUS_PATH
UHAL_INCLUDE_PATH = \
	         					-isystem$(IPBUS_PATH)/uhal/uhal/include \
	         					-isystem$(IPBUS_PATH)/uhal/log/include \
	         					-isystem$(IPBUS_PATH)/uhal/grammars/include 
UHAL_LIBRARY_PATH = \
							-L$(IPBUS_PATH)/uhal/uhal/lib \
	         					-L$(IPBUS_PATH)/uhal/log/lib \
	         					-L$(IPBUS_PATH)/uhal/grammars/lib \
							-L$(IPBUS_PATH)/extern/pugixml/pugixml-1.2/ 
else
UHAL_INCLUDE_PATH = \
	         					-isystem$(CACTUS_ROOT)/include 

UHAL_LIBRARY_PATH = \
							-L$(CACTUS_ROOT)/lib -Wl,-rpath=$(CACTUS_ROOT)/lib 
endif

ifdef UIO_UHAL_PATH
UHAL_INCLUDE_PATH += -isystem$(UIO_UHAL_PATH)/include
UHAL_LIBRARY_PATH += -Wl,-rpath=$(UIO_UHAL_PATH)/lib
else
$(error UIO_UHAL_PATH is not set!)
endif


UHAL_CXX_FLAGS = ${UHAL_INCLUDE_PATH}

UHAL_LIBRARY_FLAGS = ${UHAL_LIBRARY_PATH}




.PHONY: all _all clean _cleanall build _buildall _cactus_env

clean: _cleanall clean_version
_cleanall:
	rm -rf obj
	rm -rf bin
	rm -rf lib


all: _all
build: _all
buildall: _all
_all: _cactus_env ${LIBRARY_APOLLO_SM_DEVICE} ${LIBRARY_APOLLO_SM} ${EXE_APOLLO_SM_STANDALONE_BIN} ${LIBRARY_APOLLO_SM_PYBIND} #${EXE_APOLLO_SM_STANDALONE} 

_cactus_env:
ifdef IPBUS_PATH
	$(info using uHAL lib from user defined IPBUS_PATH=${IPBUS_PATH})
else ifdef CACTUS_ROOT
	$(info using uHAL lib from user defined CACTUS_ROOT=${CACTUS_ROOT})
else
	$(error Must define IPBUS_PATH or CACTUS_ROOT to include uHAL libraries (define through Makefile or command line\))
endif

# ------------------------
# ApolloSM library
# ------------------------
${LIBRARY_APOLLO_SM_DEVICE}: ${LIBRARY_APOLLO_SM_DEVICE_OBJECT_FILES}  ${IPBUS_REG_HELPER_PATH}/lib/libBUTool_IPBusRegHelpers.so ${IPBUS_REG_HELPER_PATH}/lib/libBUTool_IPBusIO.so ${LIBRARY_APOLLO_SM}
	${CXX} ${LINK_LIBRARY_FLAGS} -lBUTool_ApolloSM ${LIBRARY_APOLLO_SM_DEVICE_OBJECT_FILES} -o $@
	@echo "export BUTOOL_AUTOLOAD_LIBRARY_LIST=\$$BUTOOL_AUTOLOAD_LIBRARY_LIST:$(RUNTIME_LDPATH)/${LIBRARY_APOLLO_SM_DEVICE}" > env.sh

${LIBRARY_APOLLO_SM}: ${LIBRARY_APOLLO_SM_OBJECT_FILES} ${IPBUS_REG_HELPER_PATH}/lib/libBUTool_IPBusIO.so
	${CXX} ${LINK_LIBRARY_FLAGS}  ${LIBRARY_APOLLO_SM_OBJECT_FILES} -o $@


# -----------------------
# Python binding library for ApolloSM
# -----------------------
${LIBRARY_APOLLO_SM_PYBIND}: ${LIBRARY_APOLLO_SM}
	${CXX} ${CXX_FLAGS} ${UHAL_CXX_FLAGS} ${LINK_LIBRARY_FLAGS} -I${PYBIND11_PATH}/include -lBUTool_ApolloSM $(shell python3-config --includes) ${SOURCE_APOLLO_SM_PYBIND} -o $@

# -----------------------
# install
# -----------------------
install: all
	install -m 775 -d ${INSTALL_PATH}/lib
	@echo "Installing lib/* to ${INSTALL_PATH}/lib"
	install -b -m 775 ./lib/* ${INSTALL_PATH}/lib
	install -m 775 -d ${INSTALL_PATH}/bin
	@echo "Installing bin/* to ${INSTALL_PATH}/bin"
	install -b -m 775 ./bin/* ${INSTALL_PATH}/bin
	install -m 775 -d ${INSTALL_PATH}/include
	@echo "Installing include/* to ${INSTALL_PATH}/include"
	cp -r include/* ${INSTALL_PATH}/include
	@echo "Installing lib/ApolloSM*.so to ${PYTHON3_INSTALL_PATH}"
	cp lib/ApolloSM*.so ${PYTHON3_INSTALL_PATH}



obj/%.o : src/%.cc
	mkdir -p $(dir $@)
	mkdir -p {lib,obj}
	${CXX} ${CXX_FLAGS} ${UHAL_CXX_FLAGS} -c $< -o $@

obj/%.o : src/%.cxx 
	mkdir -p $(dir $@)
	mkdir -p {lib,obj}
	${CXX} ${CXX_FLAGS} ${UHAL_CXX_FLAGS} -c $< -o $@

#specific rule for peek and pokeUIO since we want them free of dynamic linking to other libraries
bin/peekUIO : src/standalone/peekUIO.cxx
	mkdir -p bin
	${CXX} ${CXX_FLAGS} -Wall -g -O3 -rdynamic -lboost_filesystem -lboost_system $^ -o $@
bin/pokeUIO : src/standalone/pokeUIO.cxx
	mkdir -p bin
	${CXX} ${CXX_FLAGS} -Wall -g -O3 -rdynamic -lboost_filesystem -lboost_system $^ -o $@
bin/findUIO : src/standalone/findUIO.cxx
	mkdir -p bin
	${CXX} ${CXX_FLAGS} -Wall -g -O3 -rdynamic -lboost_filesystem -lboost_system  $^ -o $@


bin/% : obj/standalone/%.o ${EXE_APOLLO_SM_STANDALONE_OBJECT_FILES} ${LIBRARY_APOLLO_SM}
	mkdir -p bin
	${CXX} ${LINK_EXE_FLAGS} ${UHAL_LIBRARY_FLAGS} ${UHAL_LIBRARIES} -lBUTool_ApolloSM  -lboost_system -lpugixml  $< ${EXE_APOLLO_SM_STANDALONE_OBJECT_FILES} -o $@

bin/SM_boot : obj/standalone/SM_boot.o obj/standalone/optionParsing.o obj/standalone/daemon.o ${LIBRARY_APOLLO_SM}
	mkdir -p bin
	${CXX} ${LINK_EXE_FLAGS} ${UHAL_LIBRARY_FLAGS} ${UHAL_LIBRARIES} -lBUTool_ApolloSM -lboost_system -lpugixml  $(filter-out %.so, $^)  -o $@

bin/heartbeat : obj/standalone/heartbeat.o obj/standalone/optionParsing.o obj/standalone/daemon.o ${LIBRARY_APOLLO_SM}
	mkdir -p bin
	${CXX} ${LINK_EXE_FLAGS} ${UHAL_LIBRARY_FLAGS} ${UHAL_LIBRARIES} -lBUTool_ApolloSM -lboost_system -lpugixml  $(filter-out %.so, $^)  -o $@

bin/xvc_server : obj/standalone/xvc_server.o obj/standalone/optionParsing.o obj/standalone/daemon.o ${LIBRARY_APOLLO_SM}
	mkdir -p bin
	${CXX} ${LINK_EXE_FLAGS} ${UHAL_LIBRARY_FLAGS} ${UHAL_LIBRARIES} -lBUTool_ApolloSM -lboost_system -lpugixml  $(filter-out %.so, $^)  -o $@

bin/ps_monitor : obj/standalone/ps_monitor.o obj/standalone/optionParsing.o obj/standalone/daemon.o obj/standalone/userCount.o obj/standalone/lnxSysMon.o ${LIBRARY_APOLLO_SM}
	mkdir -p bin
	${CXX} ${LINK_EXE_FLAGS} ${UHAL_LIBRARY_FLAGS} ${UHAL_LIBRARIES} -lBUTool_ApolloSM -lboost_system -lpugixml  $(filter-out %.so, $^)  -o $@

bin/htmlStatus : obj/standalone/htmlStatus.o obj/standalone/optionParsing.o obj/standalone/daemon.o ${LIBRARY_APOLLO_SM}
	mkdir -p bin
	${CXX} ${LINK_EXE_FLAGS} ${UHAL_LIBRARY_FLAGS} ${UHAL_LIBRARIES} -lBUTool_ApolloSM -lboost_system -lpugixml  $(filter-out %.so, $^)  -o $@

bin/i2c_write_monitor : obj/standalone/i2c_write_monitor.o obj/standalone/optionParsing.o obj/standalone/daemon.o ${LIBRARY_APOLLO_SM}
	mkdir -p bin
	${CXX} ${LINK_EXE_FLAGS} ${UHAL_LIBRARY_FLAGS} ${UHAL_LIBRARIES} -lBUTool_ApolloSM -lboost_system -lpugixml -lsystemd  $(filter-out %.so, $^)  -o $@


-include $(LIBRARY_OBJECT_FILES:.o=.d)

