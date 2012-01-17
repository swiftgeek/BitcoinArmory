CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(BitcoinArmory)

SET(PYTHONINTERP_PREFER_PYTHON2 TRUE)
SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(PythonInterp REQUIRED)
find_package(Twisted REQUIRED)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)
ADD_SUBDIRECTORY(CppBlockUtils CppBlockUtils/bin)

INSTALL(FILES qtmodels.py CppBlockUtils.py unittest.py manuallyCreateTx.py btcarmoryengine.py  DESTINATION share/${PROJECT_NAME})
INSTALL(FILES extras/createTestChain.py extras/mysteryHex.py DESTINATION share/${PROJECT_NAME}/extras)
INSTALL(FILES LICENSE DESTINATION share/license/${PROJECT_NAME})
INSTALL(FILES README doc/Using_PyBtcEngine.README DESTINATION share/doc/${PROJECT_NAME})
INSTALL(PROGRAMS BitcoinArmoryQt.py DESTINATION share/${PROJECT_NAME})


