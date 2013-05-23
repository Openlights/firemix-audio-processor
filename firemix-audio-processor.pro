TEMPLATE = app
CONFIG += qt debug console
TARGET = firemix-audio-processor
QT += core network
DEFINES += QT_DLL QT_NETWORK_LIB

SOURCES +=  src/main.cpp \
			src/jack_client.cpp

HEADERS +=  src/jack_client.h

LIBS += -ljack