# ============================================================================
# Monocypher - the Ed25519 verification primitive for AeroraEd25519Verifier
#
# This file is an exception to the manifest convention, and the exception is deliberate: every
# other <Module>.cmake under aerora/ sets variables only and declares no target, because the
# aggregation point owns target creation. A third-party dependency has to become a target
# somewhere, so this file creates one and aerora/CMakeLists.txt - which already creates the QML
# module target - is what includes it. It is kept beside its only consumer rather than in
# cmake/modules/, because nothing outside the Aerora overlay uses it.
#
# ----------------------------------------------------------------------------
# Why a dependency at all, and why this one
# ----------------------------------------------------------------------------
#
# The credential decision (DG-ROL installables, owner-approved) is an asymmetric signature the
# station can verify but never forge, so the station holds a public key and no secret. Qt has no
# public API for that: QCryptographicHash hashes, QMessageAuthenticationCode is symmetric, and
# OpenSSL reaches this tree only through the Android build and GStreamer's GIO TLS module - there
# is no find_package(OpenSSL) for the application and neither desktop dependency installer brings
# headers. So the primitive is absent and has to be added.
#
# Monocypher is two C files, no system dependency, identical on all five target platforms, and
# dual-licensed BSD-2-Clause OR CC0-1.0. DOWNLOAD_ONLY is used because the upstream project ships a
# Makefile rather than CMake, so the target is declared here instead of by its own build.
#
# ----------------------------------------------------------------------------
# src/optional/ is load-bearing, not optional
# ----------------------------------------------------------------------------
#
# monocypher.h's crypto_eddsa_* is "EdDSA with curve25519 + BLAKE2b" - its own comment says so - and
# is NOT RFC 8032. Nothing standard produces a signature it accepts, so a deployment could not sign
# a credential with openssl, ssh-keygen or python-cryptography. The RFC 8032 variant lives in
# src/optional/monocypher-ed25519.{c,h} and announces itself as "EdDSA with curve25519 + SHA-512".
# Both translation units are therefore required, and AeroraEd25519VerifierTest pins the choice with
# the RFC 8032 section 7.1 vectors so a future bump that moved the API cannot silently fall back to
# the incompatible one.
# ============================================================================

include_guard(GLOBAL)

if(TARGET aerora_monocypher)
    return()
endif()

if(NOT COMMAND CPMAddPackage)
    include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/modules/CPM.cmake")
endif()

CPMAddPackage(
    NAME monocypher
    VERSION 4.0.3
    GITHUB_REPOSITORY LoupVaillant/Monocypher
    GIT_TAG 4.0.3
    DOWNLOAD_ONLY YES
)

if(NOT monocypher_ADDED)
    message(FATAL_ERROR "Aerora: monocypher (the Ed25519 verification primitive) was not resolved")
endif()

add_library(aerora_monocypher STATIC ${monocypher_SOURCE_DIR}/src/monocypher.c
                                     ${monocypher_SOURCE_DIR}/src/optional/monocypher-ed25519.c
)

# PUBLIC so the overlay translation unit that includes monocypher-ed25519.h inherits both, and that
# header includes "monocypher.h" by name, so src/ has to be reachable as well as src/optional/.
target_include_directories(aerora_monocypher PUBLIC ${monocypher_SOURCE_DIR}/src ${monocypher_SOURCE_DIR}/src/optional)

# The application compiles with -Werror. A third-party C library is not ours to keep warning-clean,
# and GeographicLib.cmake applies the same helper for the same reason.
if(COMMAND qgc_disable_dependency_warnings)
    qgc_disable_dependency_warnings(aerora_monocypher)
endif()

set_target_properties(aerora_monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)
