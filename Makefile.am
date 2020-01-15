AM_CXXFLAGS = \
	-std=c++11 \
	-Wall \
	-fPIC \
	-DCXX_FLAGS="\"${DHBOC_CXXFLAGS}\"" \
	-DLD_FLAGS="\"${DHBOC_LDFLAGS}\""

lib_LTLIBRARIES = libdhboc.la
libdhboc_la_SOURCES = \
	src/modules.cpp \
	src/startup.cpp

pkginclude_HEADERS = \
	src/application.h \
	src/modules.h \
	src/startup.h

bin_PROGRAMS = dhboc
dhboc_LDADD = libdhboc.la
dhboc_SOURCES = \
	src/main.cpp
