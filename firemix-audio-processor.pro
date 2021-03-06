TEMPLATE = app
CONFIG += qt debug console
TARGET = firemix-audio-processor
QT += core network
DEFINES += QT_DLL QT_NETWORK_LIB

SOURCES +=  src/main.cpp \
			src/jack_client.cpp \
			src/networking.cpp

HEADERS +=  src/jack_client.h \
			src/networking.h

win32 {
    INCLUDEPATH += "G:\Program Files (x86)\Jack\includes" "G:\code\aubio\src"
    LIBS += -L"G:/Program Files (x86)/Jack/lib" -L"G:/code/aubio-0.4.1.win32_binary" -laubio-4 "G:/Program Files (x86)/Jack/lib/libjack.lib"
}

!win32{
    LIBS += -ljack -laubio -L/usr/local/lib
}
