VERSION_MAJOR = 0
VERSION_MINOR = 7
VERSION_PATCH = 4

DEFINES     += "VERSION_MAJOR=$$VERSION_MAJOR" \
               "VERSION_MINOR=$$VERSION_MINOR" \
               "VERSION_PATCH=$$VERSION_PATCH" \

VERSION      = $${VERSION_MAJOR}.$${VERSION_MINOR}.$${VERSION_PATCH}

QT          += core gui webenginewidgets
CONFIG      += c++11
TEMPLATE     = app

DESTDIR      = bin
PROG_NAME    = liquid

SRC_DIR      = src
INC_DIR      = inc
FORMS_DIR    = ui

OBJECTS_DIR  = .objs
MOC_DIR      = .mocs
UI_DIR       = .uis
RCC_DIR      = .qrcs

INCLUDEPATH += $${INC_DIR}

SOURCES     += src/liquidappcookiejar.cpp \
               src/liquidappcreateeditdialog.cpp \
               src/liquidappwebpage.cpp \
               src/liquidappwindow.cpp \
               src/main.cpp \
               src/mainwindow.cpp \

HEADERS     += inc/config.h \
               inc/liquidappcookiejar.hpp \
               inc/liquidappcreateeditdialog.hpp \
               inc/liquidappwebpage.hpp \
               inc/liquidappwindow.hpp \
               inc/mainwindow.hpp \

RESOURCES    = res/resources.qrc

OTHER_FILES += res/images/$${PROG_NAME}.svg \
               res/styles/$${PROG_NAME}.qss

QMAKE_CLEAN += -r $${DESTDIR}/$${PROG_NAME}

DEFINES     += PROG_NAME=\\\"$${PROG_NAME}\\\"

CONFIG      += debug

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
