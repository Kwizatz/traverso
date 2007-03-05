

#CONFIG += static
CONFIG += dll

#QMAKE_CXXFLAGS_RELEASE += -fPIC
#QMAKE_CXXFLAGS_DEBUG += -fPIC
# File generated by kdevelop's qmake manager. 
# ------------------------------------------- 
# Subdir relative project main directory: ./src/core
# Target is a library:  traversocore

include(../libbase.pri)

PRECOMPILED_HEADER = precompile.h 

LIBS += -ltraversocommands \
        -ltraversoaudiobackend

INCLUDEPATH += ../commands \
	../engine \
	../plugins \
               . 
QMAKE_LIBDIR = ../../lib 
TARGET = traversocore 
DESTDIR = ../../lib 

TEMPLATE = lib 

SOURCES = AudioClip.cpp \
	AudioClipList.cpp \
	AudioClipManager.cpp \
	AudioSource.cpp \
	AudioSourceManager.cpp \
	Command.cpp \
	Config.cpp \
	ContextItem.cpp \
	ContextPointer.cpp \
	Curve.cpp \
	CurveNode.cpp \
	Debugger.cpp \
	DiskIO.cpp \
	Export.cpp \
	FadeCurve.cpp \
	FileHelpers.cpp \
	Information.cpp \
	InputEngine.cpp \
	Mixer.cpp \
	Peak.cpp \
	PrivateReadSource.cpp \
	Project.cpp \
	ProjectManager.cpp \
	ReadSource.cpp \
	RingBuffer.cpp \
	Song.cpp \
	Track.cpp \
	Utils.cpp \
	ViewPort.cpp \
	WriteSource.cpp \
	gdither.cpp \
	SnapList.cpp \
	TimeLine.cpp \
	Marker.cpp
HEADERS = precompile.h \
	AudioClip.h \
	AudioClipList.h \
	AudioClipManager.h \
	AudioSource.h \
	AudioSourceManager.h \
	Command.h \
	Config.h \
	ContextItem.h \
	ContextPointer.h \
	CurveNode.h \
	Curve.h \
	Debugger.h \
	DiskIO.h \
	Export.h \
	FadeCurve.h \
	FileHelpers.h \
	Information.h \
	InputEngine.h \
	Mixer.h \
	Peak.h \
	PrivateReadSource.h \
	Project.h \
	ProjectManager.h \
	ReadSource.h \
	RingBuffer.h \
	RingBufferNPT.h \
	Song.h \
	Track.h \
	Utils.h \
	ViewPort.h \
	WriteSource.h \
	libtraversocore.h \
	gdither.h \
	gdither_types.h \
	gdither_types_internal.h \
	noise.h \
	FastDelegate.h \
	SnapList.h \
	CommandPlugin.h \
	TimeLine.h \
	Marker.h
macx{
    QMAKE_LIBDIR += /usr/local/qt/lib
}

SOURCES -= MultiMeter.cpp \
MtaRegionList.cpp \
MtaRegion.cpp \
ContextItem.cpp
HEADERS -= MultiMeter.h \
MtaRegion.h \
MtaRegionList.h
