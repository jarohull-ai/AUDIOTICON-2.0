//! Build script for the Audioticon <-> Qt bridge.
//!
//! Two jobs:
//!  1. compile the `#[cxx::bridge]` glue plus our own C++ (`src/bridge.cc`) into
//!     the Rust staticlib, so the Qt binary only has to link one `.a`;
//!  2. publish the generated headers to a FIXED path.
//!
//! (2) is the whole reason this file is not a one-liner. `cxx-build` writes to
//! `target/<profile>/build/<pkg>-<hash>/out/cxxbridge`, and qmake cannot predict
//! a hash. We copy the headers to `<backend>/target/cxxbridge`, which
//! `ui/src/bridge/rustlink.pri` addresses literally. The destination is derived
//! from `CARGO_MANIFEST_DIR`, so a globally exported `CARGO_TARGET_DIR` cannot
//! move the headers away from the Qt build.

use std::fs;
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

/// Directory name cargo uses for this package inside `target/<profile>/build`.
const CRATE_DIR: &str = "audioticon-bridge";

fn main() {
    cxx_build::bridge("src/lib.rs")
        .file("src/bridge.cc")
        .include("include")
        .flag_if_supported("-std=c++17")
        .compile("audioticon-bridge");

    publish_glue();

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/bridge.cc");
    println!("cargo:rerun-if-changed=include/BridgeObserver.h");
    println!("cargo:rerun-if-env-changed=CARGO_TARGET_DIR");
}

/// Copies the headers from the `cxx-build` output into
/// `<backend>/target/cxxbridge`, preserving the `audioticon-bridge/src/...`
/// layout, which is the path the generated headers include each other with.
///
/// Only headers are published: the generated `.cc` glue is already compiled into
/// the staticlib by `cxx_build`, so the Qt build must not compile it a second
/// time — that would duplicate every bridge symbol.
fn publish_glue() {
    let manifest_dir = PathBuf::from(
        std::env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR is always set by cargo"),
    );
    // <backend>/crates/bridge -> <backend>
    let backend = manifest_dir
        .parent()
        .and_then(Path::parent)
        .expect("the bridge crate is expected at <backend>/crates/bridge")
        .to_path_buf();
    let dest = backend.join("target").join("cxxbridge");

    let out_dir = PathBuf::from(std::env::var("OUT_DIR").expect("OUT_DIR is always set by cargo"));
    let Some(include) = find_glue_include(&out_dir) else {
        panic!(
            "cxx-build produced no headers reachable from {}; the Qt build would have nothing to include",
            out_dir.display()
        );
    };

    // Recreate from scratch. An older build, or a hand-made copy, may have left
    // symlinks here; `fs::copy` follows symlinks and would overwrite the link
    // target instead of replacing the link, silently publishing a stale header.
    if dest.exists() {
        fs::remove_dir_all(&dest).expect("cannot clear the previous cxx glue directory");
    }

    let copied = copy_tree(&include, &dest);
    if copied == 0 {
        panic!("no headers copied from {}", include.display());
    }

    // Preflight B2: wykrywanie pustego/uszkodzonego cxx.h
    let cxx_h = dest.join("rust").join("cxx.h");
    if !cxx_h.is_file() {
        panic!("Preflight B2 FAILED: cxx.h nie istnieje w {}", cxx_h.display());
    }
    let cxx_meta = fs::metadata(&cxx_h).expect("odczyt metadanych cxx.h");
    if cxx_meta.len() < 100 {
        panic!(
            "Preflight B2 FAILED: cxx.h jest pusty lub uszkodzony (rozmiar: {} B) w {}!",
            cxx_meta.len(),
            cxx_h.display()
        );
    }
    let cxx_content = fs::read_to_string(&cxx_h).unwrap_or_default();
    if !cxx_content.contains("namespace rust") {
        panic!(
            "Preflight B2 FAILED: cxx.h nie zawiera 'namespace rust' w {}!",
            cxx_h.display()
        );
    }

    println!(
        "cargo:warning=cxx headers ({} files, cxx.h verified valid {} B) published from {} to {}",
        copied,
        cxx_meta.len(),
        include.display(),
        dest.display()
    );
}

/// Locates the include directory `cxx-build` actually wrote to.
///
/// `cxx-build` does not necessarily use this build script's `OUT_DIR`: it keeps
/// the glue in a sibling directory keyed by package metadata, so the headers
/// survive a build-script rerun. We therefore look inside our own `OUT_DIR`
/// first, then scan the sibling `build/audioticon-bridge-*` directories and take
/// the most recently written one that actually carries our header.
fn find_glue_include(out_dir: &Path) -> Option<PathBuf> {
    let marker = format!("{CRATE_DIR}/src/lib.rs.h");
    let looks_right = |dir: &Path| dir.join(&marker).is_file();

    let own = out_dir.join("cxxbridge").join("include");
    if looks_right(&own) {
        return Some(own);
    }

    let build_root = out_dir.parent()?.parent()?.parent()?.join("build");
    let mut newest: Option<(SystemTime, PathBuf)> = None;

    for entry in fs::read_dir(&build_root).ok()?.flatten() {
        if !entry.file_name().to_string_lossy().starts_with(CRATE_DIR) {
            continue;
        }
        let candidate = entry.path().join("out").join("cxxbridge").join("include");
        if !looks_right(&candidate) {
            continue;
        }
        let stamp = fs::metadata(&candidate)
            .and_then(|meta| meta.modified())
            .unwrap_or(UNIX_EPOCH);
        if newest.as_ref().is_none_or(|(best, _)| stamp > *best) {
            newest = Some((stamp, candidate));
        }
    }

    newest.map(|(_, dir)| dir)
}

/// Recursively copies `src` into `dst`, returning the number of files written.
/// A missing source is not an error: it only means there is nothing to publish.
fn copy_tree(src: &Path, dst: &Path) -> usize {
    let entries = match fs::read_dir(src) {
        Ok(entries) => entries,
        Err(_) => return 0,
    };

    let mut written = 0usize;
    for entry in entries.flatten() {
        let path = entry.path();
        let target = dst.join(entry.file_name());

        if path.is_dir() {
            // fs::copy will not create the parent directories for us.
            if fs::create_dir_all(&target).is_ok() {
                written += copy_tree(&path, &target);
            }
        } else if path.extension().is_some_and(|ext| ext == "h") {
            // cxx-build also drops the Rust sources next to the headers for span
            // mapping; only headers are of any use to the C++ side.
            if fs::copy(&path, &target).is_ok() {
                written += 1;
            }
        }
    }
    written
}
