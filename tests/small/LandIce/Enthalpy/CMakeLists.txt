get_filename_component(testName ${CMAKE_CURRENT_SOURCE_DIR} NAME)

if (ALBANY_EPETRA AND ALBANY_IOPX)
  # Copy input file from source to binary dir
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_Enthalpy_test.yaml
                 ${CMAKE_CURRENT_BINARY_DIR}/input_Enthalpy_test.yaml COPYONLY)

  # Create the test
  add_test(${testName} ${Albany.exe} input_Enthalpy_test.yaml)
  set_tests_properties(${testName} PROPERTIES LABELS "LandIce;Epetra;Forward")
endif()
