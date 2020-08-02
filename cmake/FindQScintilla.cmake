# - Try to find the QScintilla2 includes and library
# which defines
#
# QSCINTILLA_FOUND - system has QScintilla2
# QSCINTILLA_INCLUDE_DIR - where to find qextscintilla.h
# QSCINTILLA_LIBRARIES - the libraries to link against to use QScintilla
# QSCINTILLA_LIBRARY - where to find the QScintilla library (not for general use)

# copyright (c) 2007 Thomas Moenicke thomas.moenicke@kdemail.net
# copyright (c) 2015 swaechter (unknown SO hero)
#
# Redistribution and use is allowed according to the terms of the FreeBSD license.

SET(QSCINTILLA_FOUND FALSE)

IF(QT4_FOUND)
    # macosx specific tests for frameworks and include paths
    set (FRAMEWORK_INCLUDE_DIR "")
    if (APPLE)
        # HACK to fixup macosx issue with QT_INCLUDE_DIR:
        # QT_INCLUDE_DIR /opt/local/libexec/qt4-mac/include;/opt/local/libexec/qt4-mac/lib/QtCore.framework
        # it should be only:
        # QT_INCLUDE_DIR /opt/local/libexec/qt4-mac/include
        list(LENGTH QT_INCLUDE_DIR QT_INCLUDE_DIR_LENGTH)
        if (QT_INCLUDE_DIR_LENGTH)
            list(GET QT_INCLUDE_DIR 0 FRAMEWORK_INCLUDE_DIR) 
        endif (QT_INCLUDE_DIR_LENGTH)
    endif (APPLE)


    FIND_PATH(QSCINTILLA_INCLUDE_DIR qsciglobal.h
                # standard locations
                /usr/include
                /usr/include/Qsci
                # qt4 location except mac's frameworks
                "${QT_INCLUDE_DIR}/Qsci"
                # mac's frameworks
                ${FRAMEWORK_INCLUDE_DIR}/Qsci
    )

    SET(QSCINTILLA_NAMES ${QSCINTILLA_NAMES} qscintilla2 libqscintilla2)
    FIND_LIBRARY(QSCINTILLA_LIBRARY
        NAMES ${QSCINTILLA_NAMES}
        PATHS ${QT_LIBRARY_DIR}
    )

    IF (QSCINTILLA_LIBRARY AND QSCINTILLA_INCLUDE_DIR)

        SET(QSCINTILLA_LIBRARIES ${QSCINTILLA_LIBRARY})
        SET(QSCINTILLA_FOUND TRUE)

        IF (CYGWIN)
            IF(BUILD_SHARED_LIBS)
            # No need to define QSCINTILLA_USE_DLL here, because it's default for Cygwin.
            ELSE(BUILD_SHARED_LIBS)
            SET (QSCINTILLA_DEFINITIONS -DQSCINTILLA_STATIC)
            ENDIF(BUILD_SHARED_LIBS)
        ENDIF (CYGWIN)

    ENDIF (QSCINTILLA_LIBRARY AND QSCINTILLA_INCLUDE_DIR)
ENDIF(QT4_FOUND)

# Check
IF(Qt5Widgets_FOUND)
  # Iterate over the include list of the Qt5Widgets module
  FOREACH(TEMPPATH ${Qt5Widgets_INCLUDE_DIRS})
    # Check for a Qsci directory
    FIND_PATH(QSCINTILLA_INCLUDE_DIR qsciglobal.h ${TEMPPATH}/Qsci)

    # Found - break loop
    IF(QSCINTILLA_INCLUDE_DIR)
      BREAK()
    ENDIF()
  ENDFOREACH()

  # Check
  IF(QSCINTILLA_INCLUDE_DIR)

    # Get Qt5Widgets library and cut off the library name
    GET_TARGET_PROPERTY(QT5_WIDGETSLIBRARY Qt5::Widgets LOCATION)
    GET_FILENAME_COMPONENT(QT5_WIDGETSLIBRARYPATH ${QT5_WIDGETSLIBRARY} PATH)

    # Add library
    SET(LIBRARYPATH ${QT5_WIDGETSLIBRARYPATH} "/usr/lib/" "/usr/local/lib")
    MESSAGE("QScintilla2 LIBPATH \"${LIBRARYPATH}\"")

    FIND_LIBRARY(QSCINTILLA_LIBRARY NAMES qscintilla2-qt5 qt5scintilla2 libqscintilla2_qt5.so libqscintilla2.so libqscintilla2.a qscintilla2.lib PATHS ${LIBRARYPATH})

    # Check
    IF(QSCINTILLA_LIBRARY)
      # Enable library
      SET(QSCINTILLA_LIBRARIES ${QSCINTILLA_LIBRARY})
      SET(QSCINTILLA_FOUND true)
      MARK_AS_ADVANCED(QSCINTILLA_INCLUDE_DIR QSCINTILLA_LIBRARY)
    ELSE()
      MESSAGE(FATAL_ERROR "QScintilla2 library not found")
    ENDIF()
  ELSE()
    MESSAGE(FATAL_ERROR "Cannot find QScintilla2 header")
  ENDIF()
ENDIF(Qt5Widgets_FOUND)

IF (QSCINTILLA_FOUND)
  IF (NOT QScintilla_FIND_QUIETLY)
    MESSAGE(STATUS "Found QScintilla2: ${QSCINTILLA_LIBRARY}")
    MESSAGE(STATUS "         includes: ${QSCINTILLA_INCLUDE_DIR}")
  ENDIF (NOT QScintilla_FIND_QUIETLY)
ELSE (QSCINTILLA_FOUND)
  IF (QScintilla_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could not find QScintilla library")
  ENDIF (QScintilla_FIND_REQUIRED)
ENDIF (QSCINTILLA_FOUND)

MARK_AS_ADVANCED(QSCINTILLA_INCLUDE_DIR QSCINTILLA_LIBRARY)

