!win:!mac {
    LIBS += -lX11 -lXfixes -lXinerama -lXext -lXcomposite
}

QT           += core
CONFIG       += c++11
TEMPLATE      = lib

INC_DIR       = inc
SRC_DIR       = src

INCLUDEPATH  += $${PWD}/$${INC_DIR}

HEADERS      += $${PWD}/inc/singleinstance.hpp

SOURCES      += $${PWD}/src/singleinstance.cpp
