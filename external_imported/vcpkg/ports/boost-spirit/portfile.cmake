# Automatically generated by scripts/boost/generate-ports.ps1

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO boostorg/spirit
    REF boost-1.75.0
    SHA512 83d4cb3aad1e635e14640dace19d44db7886a0e25a30f9c5ff5384c5be8f6961231f947f4df0baf07f9bbda6893988ed9099700d596f99ff39ea22151e436912
    HEAD_REF master
)

include(${CURRENT_INSTALLED_DIR}/share/boost-vcpkg-helpers/boost-modular-headers.cmake)
boost_modular_headers(SOURCE_PATH ${SOURCE_PATH})
