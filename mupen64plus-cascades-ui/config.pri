# Auto-generated by IDE. All changes by user will be lost!
# Created at 2/23/13 2:16 PM

BASEDIR = $$_PRO_FILE_PWD_

INCLUDEPATH +=  \
    $$BASEDIR/src

SOURCES +=  \
    $$BASEDIR/src/NetRequest.cpp \
    $$BASEDIR/src/cheat.c \
    $$BASEDIR/src/compare_core.c \
    $$BASEDIR/src/core_interface.c \
    $$BASEDIR/src/emulator.cpp \
    $$BASEDIR/src/frontend.cpp \
    $$BASEDIR/src/imageloader.cpp \
    $$BASEDIR/src/imageprocessor.cpp \
    $$BASEDIR/src/main.cpp \
    $$BASEDIR/src/osal_dynamiclib_unix.c \
    $$BASEDIR/src/osal_files_unix.c \
    $$BASEDIR/src/plugin.c

HEADERS +=  \
    $$BASEDIR/src/NetRequest.hpp \
    $$BASEDIR/src/cheat.h \
    $$BASEDIR/src/compare_core.h \
    $$BASEDIR/src/core_interface.h \
    $$BASEDIR/src/emulator.h \
    $$BASEDIR/src/frontend.h \
    $$BASEDIR/src/imageloader.hpp \
    $$BASEDIR/src/imageprocessor.hpp \
    $$BASEDIR/src/main.h \
    $$BASEDIR/src/osal_dynamiclib.h \
    $$BASEDIR/src/osal_files.h \
    $$BASEDIR/src/osal_preproc.h \
    $$BASEDIR/src/plugin.h \
    $$BASEDIR/src/version.h

lupdate_inclusion {
    SOURCES += \
        $$BASEDIR/../assets/*.qml
}

TRANSLATIONS = \
    $${TARGET}.ts

