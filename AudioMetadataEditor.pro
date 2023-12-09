QT += core gui widgets QtMultimedia
CONFIG += qtmultimedia

TARGET = AudioMetadataEditor
TEMPLATE = app

SOURCES += main.cpp \
           wavfile.cpp

HEADERS += wavfile.h

# Additional libraries to link
LIBS += -ltaglib -framework AudioToolbox -framework AVFoundation -framework CoreAudio -framework CoreMedia

# Qt Multimedia module path
INCLUDEPATH += /usr/include/qt/QtMultimedia
