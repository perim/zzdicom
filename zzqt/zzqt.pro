QT += core gui xml opengl
TARGET = zzqt
TEMPLATE = app debug
SOURCES += main.cpp mainwindow.cpp ../zz.c ../part6.c ../zztexture.c ../zzio.c
HEADERS += mainwindow.h ../zz_priv.h ../zz.h ../zztexture.h ../zzio.h
FORMS += mainwindow.ui
LIBS += -lCharLS -luuid
