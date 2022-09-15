VERSION_MAJOR = 0
VERSION_MINOR = 7
VERSION_PATCH = 11

VERSION      = $${VERSION_MAJOR}.$${VERSION_MINOR}.$${VERSION_PATCH}

QT          += core gui webenginewidgets
CONFIG      += c++11
TEMPLATE     = app

DESTDIR      = build
PROG_NAME    = liquid

SRC_DIR      = src
INC_DIR      = inc
FORMS_DIR    = ui

OBJECTS_DIR  = .objs
MOC_DIR      = .mocs
UI_DIR       = .uis
RCC_DIR      = .qrcs

INCLUDEPATH += $${INC_DIR}

SOURCES     += src/liquid.cpp \
               src/liquidappcookiejar.cpp \
               src/liquidappconfigwindow.cpp \
               src/liquidappwebpage.cpp \
               src/liquidappwindow.cpp \
               src/main.cpp \
               src/mainwindow.cpp \

HEADERS     += inc/lqd.h \
               inc/liquid.hpp \
               inc/liquidappcookiejar.hpp \
               inc/liquidappconfigwindow.hpp \
               inc/liquidappwebpage.hpp \
               inc/liquidappwindow.hpp \
               inc/mainwindow.hpp \

RESOURCES    = res/resources.qrc

OTHER_FILES += res/images/$${PROG_NAME}.svg \
               res/styles/base.qss \
               res/styles/dark.qss \
               res/styles/light.qss

QMAKE_CLEAN += -r $${DESTDIR}/$${PROG_NAME}

CONFIG      += debug

DEFINES     += PROG_NAME=\\\"$${PROG_NAME}\\\"

DEFINES     += "VERSION_MAJOR=$$VERSION_MAJOR" \
               "VERSION_MINOR=$$VERSION_MINOR" \
               "VERSION_PATCH=$$VERSION_PATCH" \

DEFINES     += VERSION=\\\"$${VERSION}\\\"

# GNU/Linux, FreeBSD, and similar
unix:!mac {
    isEmpty(PREFIX) {
        PREFIX = /usr
    }
    BINDIR = $${PREFIX}/bin
    DATADIR =$${PREFIX}/share

    OTHER_FILES += dist/$${PROG_NAME}.desktop

    target.path = $${BINDIR}

    INSTALLS += target

    desktop.path = $${DATADIR}/applications
    eval(desktop.files += dist/$${PROG_NAME}.desktop)

    INSTALLS += desktop

    icon.path = $${DATADIR}/icons/hicolor/scalable/apps
    eval(icon.files += res/images/$${PROG_NAME}.svg)

    INSTALLS += icon
}
