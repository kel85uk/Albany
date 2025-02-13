IF (SEACAS_EPU AND SEACAS_EXODIFF)
  get_filename_component(testName ${CMAKE_CURRENT_SOURCE_DIR} NAME)

  # Copy mesh files from source to binary dir
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/coupon.gen ${CMAKE_CURRENT_BINARY_DIR}/coupon.gen COPYONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/coupon.gen.4.0 ${CMAKE_CURRENT_BINARY_DIR}/coupon.gen.4.0 COPYONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/coupon.gen.4.1 ${CMAKE_CURRENT_BINARY_DIR}/coupon.gen.4.1 COPYONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/coupon.gen.4.2 ${CMAKE_CURRENT_BINARY_DIR}/coupon.gen.4.2 COPYONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/coupon.gen.4.3 ${CMAKE_CURRENT_BINARY_DIR}/coupon.gen.4.3 COPYONLY)

  IF (ALBANY_EPETRA) 
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/coupon.yaml ${CMAKE_CURRENT_BINARY_DIR}/coupon.yaml COPYONLY)
    createExoDiffAlgebraTest(${testName}_Epetra coupon.yaml coupon)
    set_tests_properties(${testName}_Epetra PROPERTIES LABELS "ATO;Epetra;Forward")
  ENDIF() 

  IF (ALBANY_IFPACK2)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/couponT.yaml ${CMAKE_CURRENT_BINARY_DIR}/couponT.yaml COPYONLY)
    createExoDiffAlgebraTest(${testName}_Tpetra couponT.yaml couponT)
    set_tests_properties(${testName}_Tpetra PROPERTIES LABELS "ATO;Tpetra;Forward")
  ENDIF()
ENDIF()
