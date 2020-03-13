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
	src/template.cpp \
	src/dhboc.cpp \
	src/redismgr.cpp \
	src/redisobj.cpp \
	src/session.cpp \
	src/html.cpp

pkginclude_HEADERS = \
	src/dhboc.h \
	src/modules.h \
	src/startup.h \
	src/handler.h \
	src/template.h \
	src/redismgr.h \
	src/redisobj.h \
	src/session.h \
	src/html.h

bin_PROGRAMS = dhboc
dhboc_LDADD = libdhboc.la
dhboc_SOURCES = \
	src/main.cpp
