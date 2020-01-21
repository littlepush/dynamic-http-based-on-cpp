AM_CXXFLAGS = \
	-std=c++11 \
	-Wall \
	-fPIC \
	-DCXX_FLAGS="\"${DHBOC_CXXFLAGS}\"" \
	-DLD_FLAGS="\"${DHBOC_LDFLAGS}\""

lib_LTLIBRARIES = libdhboc.la
libdhboc_la_SOURCES = \
	src/modules.cpp \
	src/startup.cpp \
	src/handler.cpp \
	src/dhboc.cpp

pkginclude_HEADERS = \
	src/dhboc.h \
	src/modules.h \
	src/startup.h \
	src/handler.h

bin_PROGRAMS = dhboc
dhboc_LDADD = libdhboc.la
dhboc_SOURCES = \
	src/main.cpp
