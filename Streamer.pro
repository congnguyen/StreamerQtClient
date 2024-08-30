QT += core

CONFIG += c++17 cmdline

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += /Users/congnguyen/Documents/MyWork/StreamLibs/libdatachannel/include \
                /Users/congnguyen/Documents/MyWork/StreamLibs/libdatachannel/deps/json/include

INCLUDEPATH += /Users/congnguyen/Documents/MyWork/StreamLibs/ffmpeg-6.1/build/include

# Link FFmpeg libraries
LIBS += -L/Users/congnguyen/Documents/MyWork/StreamLibs/ffmpeg-6.1/build/lib \
    -lavformat \
    -lavcodec \
    -lavutil \
    -lavdevice \
    -lswscale \
    -lswresample

SOURCES += \
        ArgParser.cpp \
        dispatchqueue.cpp \
        fileparser.cpp \
        h264fileparser.cpp \
        helpers.cpp \
        main.cpp \
        opusfileparser.cpp \
        rtspcapture.cpp \
        stream.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    ArgParser.hpp \
    dispatchqueue.hpp \
    fileparser.hpp \
    h264fileparser.hpp \
    helpers.hpp \
    opusfileparser.hpp \
    rtspcapture.h \
    stream.hpp


macx: LIBS += -L$$PWD/libs/ -ldatachannel.0

INCLUDEPATH += $$PWD/libs/include
DEPENDPATH += $$PWD/libs/include
