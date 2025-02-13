get_filename_component(testName ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Copy mesh files from source to binary dir
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mitchell3D.gen     ${CMAKE_CURRENT_BINARY_DIR}/mitchell3D.gen COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mitchell3D.gen.4.0 ${CMAKE_CURRENT_BINARY_DIR}/mitchell3D.gen.4.0 COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mitchell3D.gen.4.1 ${CMAKE_CURRENT_BINARY_DIR}/mitchell3D.gen.4.1 COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mitchell3D.gen.4.2 ${CMAKE_CURRENT_BINARY_DIR}/mitchell3D.gen.4.2 COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mitchell3D.gen.4.3 ${CMAKE_CURRENT_BINARY_DIR}/mitchell3D.gen.4.3 COPYONLY)

IF (ALBANY_EPETRA)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/nodal.yaml ${CMAKE_CURRENT_BINARY_DIR}/nodal.yaml COPYONLY)
  add_test(${testName}_Epetra ${Albany.exe} nodal.yaml)
  set_tests_properties(${testName}_Epetra PROPERTIES LABELS "ATO;Epetra;Forward")
ENDIF() 

IF (ALBANY_IFPACK2)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/nodalT.yaml ${CMAKE_CURRENT_BINARY_DIR}/nodalT.yaml COPYONLY)
  add_test(${testName}_Tpetra ${Albany.exe} nodalT.yaml)
  set_tests_properties(${testName}_Tpetra PROPERTIES LABELS "ATO;Tpetra;Forward")
ENDIF()
