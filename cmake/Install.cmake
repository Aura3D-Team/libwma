# One shared prefix for every platform: headers install once to include/, the
# library carries its ABI tag, and each build drops its own targets file beside
# a config that selects the caller's.
install(
    TARGETS wma
    EXPORT wmaTargets

    FILE_SET public_headers
    DESTINATION include

    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(
    EXPORT wmaTargets
    FILE wma-targets-${WMA_PLATFORM_SUFFIX}.cmake
    NAMESPACE wma::
    DESTINATION lib/cmake/wma
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/wmaConfig.cmake.in
    ${PROJECT_BINARY_DIR}/wmaConfig.cmake
    INSTALL_DESTINATION lib/cmake/wma
)

# This build's own find_dependency() calls, keyed by ABI tag alongside its
# targets file -- see cmake/wma-deps.cmake.in for why they cannot sit in the
# shared config.
configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/wma-deps.cmake.in
    ${PROJECT_BINARY_DIR}/wma-deps-${WMA_PLATFORM_SUFFIX}.cmake
    @ONLY
)

# ARCH_INDEPENDENT: one version file serves every platform here, and the default
# stamps the building machine's word size into it -- a wasm32 install would then
# be rejected by a 64-bit consumer. ABI matching is the targets suffix's job.
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/wmaConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

install(
    FILES
    ${PROJECT_BINARY_DIR}/wmaConfig.cmake
    ${PROJECT_BINARY_DIR}/wmaConfigVersion.cmake
    ${PROJECT_BINARY_DIR}/wma-deps-${WMA_PLATFORM_SUFFIX}.cmake
    ${PROJECT_SOURCE_DIR}/cmake/PlatformSuffix.cmake
    DESTINATION
    lib/cmake/wma
)

# Apache-2.0 4(a)/(d) and the third-party notices: they must reach every
# archive built from this install tree.
install(
    FILES
        ${PROJECT_SOURCE_DIR}/LICENSE
        ${PROJECT_SOURCE_DIR}/NOTICE
        ${PROJECT_SOURCE_DIR}/THIRD_PARTY_LICENSES.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
