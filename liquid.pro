VERSION_MAJOR = 0
VERSION_MINOR = 7
VERSION_PATCH = 11

VERSION      = $${VERSION_MAJOR}.$${VERSION_MINOR}.$${VERSION_PATCH}

QT          += core gui network svg widgets webenginecore webenginewidgets
CONFIG      += c++17
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

HEADERS     += inc/lqd.h \
               inc/liquid.hpp \
               inc/liquidappfile.hpp \
               inc/liquidappfind.hpp \
               inc/liquidappicon.hpp \
               inc/liquidapplauncher.hpp \
               inc/liquidapplocalstorage.hpp \
               inc/liquidappcookiejar.hpp \
               inc/liquidappconfigwindow.hpp \
               inc/liquidappwebpage.hpp \
               inc/liquidappwindow.hpp \
               inc/mainwindow.hpp \
               inc/singleinstance.hpp \
               inc/svgsnapshot.hpp \

SOURCES     += src/liquid.cpp \
               src/liquidappfile.cpp \
               src/liquidappfind.cpp \
               src/liquidappicon.cpp \
               src/liquidapplauncher.cpp \
               src/liquidapplocalstorage.cpp \
               src/liquidappcookiejar.cpp \
               src/liquidappconfigwindow.cpp \
               src/liquidappwebpage.cpp \
               src/liquidappwindow.cpp \
               src/main.cpp \
               src/mainwindow.cpp \
               src/singleinstance.cpp \
               src/svgsnapshot.cpp \

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
    # Needed for bringing windows to front under X11
    HEADERS += inc/liquidx11.h
    SOURCES += src/liquidx11.cpp
    LIBS    += -lX11

    # Bringing windows to front on Wayland requires private Qt API (6.5+), use it if available
    greaterThan(QT_MAJOR_VERSION, 5) {
        exists($$[QT_INSTALL_HEADERS]/QtGui/$$[QT_VERSION]/QtGui/qpa/qplatformwindow_p.h) {
            QT      += gui-private
            DEFINES += LQD_WAYLAND_ACTIVATION
        } else {
            message("Qt's private headers not found (e.g. qt6-base-private-dev), windows won't be raised on Wayland")
        }
    }

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

# macOS
macx: {
    HEADERS += inc/SetWindowBackgroundColor.h
    SOURCES += src/SetWindowBackgroundColor.mm

    LIBS += -framework Cocoa

    QMAKE_CLEAN += -r $${DESTDIR}/$${TARGET}.app \
                      $${DESTDIR}/$${TARGET}.icns \
                      $${DESTDIR}/$${TARGET}.iconset

    !exists($${DESTDIR}/$${TARGET}.icns) {
        QMAKE_PRE_LINK += mkdir -p $${DESTDIR}/$${TARGET}.iconset &&
        QMAKE_PRE_LINK += convert -density 5.64705882353 -background none -resize 16x16 $${PWD}/res/images/$${PROG_NAME}.svg $${DESTDIR}/$${TARGET}.iconset/icon_16x16.png &&
        QMAKE_PRE_LINK += convert -density 11.2941176471 -background none -resize 32x32 $${PWD}/res/images/$${PROG_NAME}.svg $${DESTDIR}/$${TARGET}.iconset/icon_16x16@2x.png &&
        QMAKE_PRE_LINK += cp $${DESTDIR}/$${TARGET}.iconset/icon_16x16@2x.png $${DESTDIR}/$${TARGET}.iconset/icon_32x32.png &&
        QMAKE_PRE_LINK += convert -density 22.5882352941 -background none -resize 64x64 $${PWD}/res/images/$${PROG_NAME}.svg $${DESTDIR}/$${TARGET}.iconset/icon_32x32@2x.png &&
        QMAKE_PRE_LINK += cp $${DESTDIR}/$${TARGET}.iconset/icon_32x32@2x.png $${DESTDIR}/$${TARGET}.iconset/icon_64x64.png &&
        QMAKE_PRE_LINK += convert -density 45.1764705882 -background none -resize 128x128 $${PWD}/res/images/$${PROG_NAME}.svg $${DESTDIR}/$${TARGET}.iconset/icon_64x64@2x.png &&
        QMAKE_PRE_LINK += cp $${DESTDIR}/$${TARGET}.iconset/icon_64x64@2x.png $${DESTDIR}/$${TARGET}.iconset/icon_128x128.png &&
        QMAKE_PRE_LINK += convert -density 90.3529411765 -background none -resize 256x256 $${PWD}/res/images/$${PROG_NAME}.svg $${DESTDIR}/$${TARGET}.iconset/icon_128x128@2x.png &&
        QMAKE_PRE_LINK += cp $${DESTDIR}/$${TARGET}.iconset/icon_128x128@2x.png $${DESTDIR}/$${TARGET}.iconset/icon_256x256.png &&
        QMAKE_PRE_LINK += iconutil -c icns $${DESTDIR}/$${TARGET}.iconset
    }

    # Qt 6 no longer creates Contents/Resources inside the app bundle on its own
    QMAKE_POST_LINK += mkdir -p $${DESTDIR}/$${TARGET}.app/Contents/Resources &&
    QMAKE_POST_LINK += cp $${DESTDIR}/$${TARGET}.icns $${DESTDIR}/$${TARGET}.app/Contents/Resources/
}
