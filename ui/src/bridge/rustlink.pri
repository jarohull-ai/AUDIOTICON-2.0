# rustlink.pri — links the Rust staticlib (crates/bridge) into Qt targets.
#
# Build the Rust side first, from backend/:
#     cargo build -p audioticon-bridge
#
# build.rs publishes the cxx headers to a fixed path (backend/target/cxxbridge),
# so this file never has to know the hashed build/... directory.
AUDIOTICON_BACKEND = $$clean_path($$PWD/../../../backend)

# B3: profil linkowania Rust dostosowany do konfiguracji Qt (release -> release staticlib)
isEmpty(AUDIOTICON_RUST_PROFILE) {
    CONFIG(release, debug|release) {
        AUDIOTICON_RUST_PROFILE = release
        CARGO_BUILD_FLAG = --release
    } else {
        AUDIOTICON_RUST_PROFILE = debug
        CARGO_BUILD_FLAG =
    }
} else {
    equals(AUDIOTICON_RUST_PROFILE, "release") {
        CARGO_BUILD_FLAG = --release
    } else {
        CARGO_BUILD_FLAG =
    }
}

AUDIOTICON_RUST_LIBDIR = $$AUDIOTICON_BACKEND/target/$$AUDIOTICON_RUST_PROFILE
AUDIOTICON_CXX_GLUE = $$AUDIOTICON_BACKEND/target/cxxbridge
AUDIOTICON_BRIDGE_INCLUDE = $$AUDIOTICON_BACKEND/crates/bridge/include

# Self-healing: if headers are missing, rebuild the bridge crate
AUDIOTICON_GLUE_HEADER = $$AUDIOTICON_CXX_GLUE/audioticon-bridge/src/lib.rs.h
!exists($$AUDIOTICON_GLUE_HEADER) {
    message("rustlink: brak opublikowanego glue cxx - przebudowuje audioticon-bridge ($$AUDIOTICON_RUST_PROFILE)")
    system("cd $$shell_quote($$AUDIOTICON_BACKEND) && cargo clean -p audioticon-bridge >/dev/null 2>&1 && cargo build -p audioticon-bridge $$CARGO_BUILD_FLAG")
}

# Preflight B2: wykrywanie pustego/uszkodzonego cxx.h (0 B lub brak namespace rust)
AUDIOTICON_CXX_H = $$AUDIOTICON_CXX_GLUE/rust/cxx.h
!exists($$AUDIOTICON_CXX_H) {
    message("rustlink: brak cxx.h - regeneruje audioticon-bridge ($$AUDIOTICON_RUST_PROFILE)...")
    system("cd $$shell_quote($$AUDIOTICON_BACKEND) && cargo clean -p audioticon-bridge >/dev/null 2>&1 && cargo build -p audioticon-bridge $$CARGO_BUILD_FLAG")
}

!system("test -s $$shell_quote($$AUDIOTICON_CXX_H) && grep -q 'namespace rust' $$shell_quote($$AUDIOTICON_CXX_H)") {
    error("B2 Preflight FAILED: naglowek $${AUDIOTICON_CXX_H} jest pusty lub uszkodzony (brak 'namespace rust')! Uruchom: (cd backend && cargo build -p audioticon-bridge $$CARGO_BUILD_FLAG)")
}

!exists($$AUDIOTICON_GLUE_HEADER) {
    error("Brak naglowkow cxx w $${AUDIOTICON_CXX_GLUE} - uruchom recznie: (cd backend && cargo build -p audioticon-bridge $$CARGO_BUILD_FLAG)")
}

!exists($$AUDIOTICON_RUST_LIBDIR/libaudioticon_bridge.a) {
    message("rustlink: brak libaudioticon_bridge.a w $$AUDIOTICON_RUST_LIBDIR - kompiluje...")
    system("cd $$shell_quote($$AUDIOTICON_BACKEND) && cargo build -p audioticon-bridge $$CARGO_BUILD_FLAG")
}

!exists($$AUDIOTICON_RUST_LIBDIR/libaudioticon_bridge.a) {
    error("Brak libaudioticon_bridge.a w $${AUDIOTICON_RUST_LIBDIR} - uruchom: (cd backend && cargo build -p audioticon-bridge $$CARGO_BUILD_FLAG)")
}
!exists($$AUDIOTICON_BRIDGE_INCLUDE/BridgeObserver.h) {
    error("Brak BridgeObserver.h w $${AUDIOTICON_BRIDGE_INCLUDE} wymaganego przez naglowek cxx")
}

INCLUDEPATH += $$AUDIOTICON_BRIDGE_INCLUDE $$AUDIOTICON_CXX_GLUE $$PWD

HEADERS += $$PWD/AudioticonCore.h
SOURCES += $$PWD/AudioticonCore.cpp

# -lpthread/-ldl/-lm are required by Rust std; -ldl also backs ort's load-dynamic.
LIBS += -L$$AUDIOTICON_RUST_LIBDIR -laudioticon_bridge -lpthread -ldl -lm
