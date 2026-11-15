QT += core gui qml quick quickcontrols2 testlib network sql concurrent
CONFIG += c++17 testcase
TARGET = hotbox_tests
TEMPLATE = app
INCLUDEPATH += ../src
HEADERS += ../src/backend.h ../src/theme.h ../src/translator.h
SOURCES += hotbox_tests.cpp ../src/backend.cpp ../src/theme.cpp ../src/translator.cpp
RESOURCES += ../src/resources.qrc

HEADERS += ../src/sessionledger.h ../src/gitworkspace.h
SOURCES += ../src/sessionledger.cpp ../src/gitworkspace.cpp

HEADERS += ../src/segmentstore.h
SOURCES += ../src/segmentstore.cpp

HEADERS += ../src/architecturestore.h
SOURCES += ../src/architecturestore.cpp
