QT += core gui qml quick quickcontrols2 network sql concurrent
CONFIG += c++17 release
TARGET = hotbox
TEMPLATE = app
INCLUDEPATH += /usr/include/LayerShellQt
LIBS += -lLayerShellQtInterface
HEADERS += src/backend.h src/theme.h src/translator.h
SOURCES += src/main.cpp src/backend.cpp src/theme.cpp src/translator.cpp
RESOURCES += src/resources.qrc

HEADERS += src/sessionledger.h src/gitworkspace.h
SOURCES += src/sessionledger.cpp src/gitworkspace.cpp
