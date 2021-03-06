CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(BitcoinArmory)
SET(CMake_VERSION_MAJOR 0)
SET(CMake_VERSION_MINOR 5)
SET(CMake_VERSION_PATCH 0)

SET(PYTHONINTERP_PREFER_PYTHON2 TRUE)
SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(PythonInterp REQUIRED)
find_package(Twisted REQUIRED)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)
ADD_SUBDIRECTORY(CppBlockUtils CppBlockUtils/bin)

# Install

INSTALL(FILES manuallyCreateTx.py qlabelbutton.py qrcodenative.py qt4reactor.py qtdefines.py qtdialogs.py unittest.py armoryengine.py armorymodels.py convertWalletVersion.py  DESTINATION share/${PROJECT_NAME})
INSTALL(FILES extras/createTestChain.py extras/mysteryHex.py extras/extractKeysFromWallet.py DESTINATION share/${PROJECT_NAME}/extras)
INSTALL(FILES LICENSE DESTINATION share/license/${PROJECT_NAME})
INSTALL(FILES README doc/Using_PyBtcEngine.README doc/Using_Armory.README DESTINATION share/doc/${PROJECT_NAME})
INSTALL(PROGRAMS ArmoryQt.py DESTINATION share/${PROJECT_NAME})

### IMGS 
FILE(GLOB IMGfiles "${CMAKE_CURRENT_SOURCE_DIR}/img/*")
INSTALL(FILES ${IMGfiles} DESTINATION share/${PROJECT_NAME}/img)

### INSTALLER SECTION - win32 and macosx only ###

SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Python-Based Bitcoin Software")
SET(CPACK_PACKAGE_VENDOR "Alan Reiner")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "CMake ${CMake_VERSION_MAJOR}.${CMake_VERSION_MINOR}")

if (WIN32 AND NOT UNIX)
  SET(CPACK_NSIS_URL_INFO_ABOUT "http:\\\\\\\\https://github.com/etotheipi/BitcoinArmory")
elseif (APPLE AND ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  message(lol)
endif()

if ( (WIN32 AND NOT UNIX) OR (APPLE AND ${CMAKE_SYSTEM_NAME} MATCHES "Darwin") )
  include(CPack)
endif()
