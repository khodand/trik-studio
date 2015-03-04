QREAL_EDITOR_NAME = "qreal"

CONFIG += C++11

SOURCES += pluginInterface.cpp

HEADERS += pluginInterface.h \
	elements.h

#INCLUDEPATH += ../../../qrgui \
#               ../../../qrgui/editorPluginInterface \

INCLUDEPATH += $$PWD/../../../qrgui/editorPluginInterface \
               $$PWD/../../../qrgui \
               $$PWD/../../../qrkernel \
               $$PWD/../../.. \

RESOURCES += plugin.qrc

include (../../../qrmc/plugins/pluginsCommon.pri)

