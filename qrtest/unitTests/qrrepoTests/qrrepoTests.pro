TARGET = qrrepo_unittests

include(../common.pri)

include(../../../qrrepo/qrrepo.pri)

SOURCES += \
	objectTest.cpp \
	graphicalObjectTest.cpp \
	folderCompressorTest.cpp \
	serializerTest.cpp \
	repositoryTest.cpp \
	repoApiTest.cpp \

HEADERS += \
	folderCompressorTest.h \
	serializerTest.h \
	repositoryTest.h \
	repoApiTest.h \
