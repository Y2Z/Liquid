include($${PWD}/modules/modules.pri)

PROG_NAME     = liquid

VERSION_MAJOR = 0
VERSION_MINOR = 7
VERSION_PATCH = 11

VERSION      = $${VERSION_MAJOR}.$${VERSION_MINOR}.$${VERSION_PATCH}

QT          += core gui webenginewidgets
CONFIG      += c++11
TEMPLATE     = app

DESTDIR      = build

INC_DIR      = inc
SRC_DIR      = src
FORMS_DIR    = ui

OBJECTS_DIR  = .objs
MOC_DIR      = .mocs
UI_DIR       = .uis
RCC_DIR      = .qrcs

INCLUDEPATH += $${INC_DIR}

HEADERS     += $${INC_DIR}/lqd.h \
               $${INC_DIR}/liquid.hpp \
               $${INC_DIR}/liquidappcookiejar.hpp \
               $${INC_DIR}/liquidappconfigwindow.hpp \
               $${INC_DIR}/liquidappwebpage.hpp \
               $${INC_DIR}/liquidappwindow.hpp \
               $${INC_DIR}/mainwindow.hpp \

SOURCES     += $${SRC_DIR}/liquid.cpp \
               $${SRC_DIR}/liquidappcookiejar.cpp \
               $${SRC_DIR}/liquidappconfigwindow.cpp \
               $${SRC_DIR}/liquidappwebpage.cpp \
               $${SRC_DIR}/liquidappwindow.cpp \
               $${SRC_DIR}/main.cpp \
               $${SRC_DIR}/mainwindow.cpp \

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
