 

# 1. Copy Input file from source to binary dir
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_BE_spectral_np3_T.yaml
               ${CMAKE_CURRENT_BINARY_DIR}/input_BE_spectral_np3_T.yaml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_BE_spectral_np4_T.yaml
               ${CMAKE_CURRENT_BINARY_DIR}/input_BE_spectral_np4_T.yaml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_BE_spectral_np5_T.yaml
               ${CMAKE_CURRENT_BINARY_DIR}/input_BE_spectral_np5_T.yaml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_RK4_spectral_np3_T.yaml
               ${CMAKE_CURRENT_BINARY_DIR}/input_RK4_spectral_np3_T.yaml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_RK4_spectral_np4_T.yaml
               ${CMAKE_CURRENT_BINARY_DIR}/input_RK4_spectral_np4_T.yaml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/input_RK4_spectral_np5_T.yaml
               ${CMAKE_CURRENT_BINARY_DIR}/input_RK4_spectral_np5_T.yaml COPYONLY)
# 2. Name the test with the directory name
get_filename_component(testName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
# 3. Create the test with this name and standard executable
#if (AERAS_IMPLICIT_HS)
#add_test(Aeras_${testName}_Spectral_np3_BackwardEuler ${Albany.exe} input_BE_spectral_np3_T.yaml)
#endif()
#add_test(Aeras_${testName}_Spectral_np3_RungeKutta ${Albany.exe} input_RK4_spectral_np3_T.yaml)

