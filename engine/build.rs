// Hands needle's package version (../VERSION, bumped by release-please) to the engine as NEEDLE_VERSION.
use std::path::Path;

fn main() {
    let file = Path::new(env!("CARGO_MANIFEST_DIR")).join("../VERSION");
    println!("cargo:rerun-if-changed={}", file.display());
    let version = std::fs::read_to_string(&file).unwrap_or_else(|e| panic!("reading {}: {e}", file.display()));
    println!("cargo:rustc-env=NEEDLE_VERSION={}", version.trim());
}
