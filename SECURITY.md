# Security Policy

Audioticon 2.0 is an offline-first, local audio workstation designed to process scripts, voice models, and audio synthesis completely on the user's workstation. Because Audioticon 2.0 does not expose HTTP daemon endpoints or run web servers, its security model focuses on local file isolation, safe memory boundaries, and explicit consent for external resources.

---

## 1. Supported Versions

| Version | Status | Supported |
|---|---|---|
| 2.0.x (Current Milestone M19) | Active Development / Pre-release | Yes |
| 1.x (Legacy Node/Web runtime) | Deprecated | No |

---

## 2. Threat Model and Security Architecture

Audioticon 2.0 adheres to strict security constraints:

1. **Local-First & Offline Boundary:**
   - The application does not spawn background HTTP/WebSocket listening servers.
   - All voice rendering, DSP filtering, and text normalization happen locally in native code.
   - The UI does not use `QWebEngine` or any embedded browser engine.

2. **Memory Safety & Language Interop:**
   - The domain core, job registry, catalog scanner, and staging pipeline are authored in memory-safe Rust.
   - Interoperability between Qt/C++ and Rust is governed exclusively by `cxx`, preventing raw pointer corruption, unchecked ABI shifts, and cross-thread ownership races.

3. **Model Import & Network Consent:**
   - **No Background Downloads:** The application never downloads models, presets, or weights automatically.
   - **Explicit Consent Gate:** Every remote asset download (GitHub Releases or Hugging Face) requires explicit user confirmation in a dedicated Glass modal displaying the author, license, byte size, file URLs, and checksums.
   - **Strict Staging & Verification:** Downloaded or imported files are extracted into isolated staging paths (`.staging/<import-id>`). Files are verified for SHA-256 and structural integrity before being moved into the permanent catalog.
   - **Executable Rejection:** Binaries (`.exe`, `.dll`, `.so`), script files (`.py`, `.sh`, `.bat`, `.js`), symbolic links, and path traversal strings (`../`) are unconditionally rejected during local and archive imports.

---

## 3. Reporting a Vulnerability

If you discover a security vulnerability or design defect that could compromise user data, break local sandbox boundaries, or execute untrusted code:

1. **Do not disclose the issue publicly.** Do not open public GitHub issues or public discussions with reproduction exploits.
2. **Submit a Private Vulnerability Report:**
   - Use GitHub's [Private Vulnerability Reporting](https://github.com/jarohull-ai/AUDIOTICON-2.0/security/advisories/new) feature on the repository.
   - Alternatively, email: `security@jarohull.ai` (or repository maintainers) with the subject `[SECURITY] Audioticon 2.0 Vulnerability Report`.
3. **Include in your report:**
   - Description of the vulnerability.
   - Affected milestone, commit SHA, and platform (Linux distribution, Qt version, Rust toolchain).
   - Minimal reproduction steps or proof-of-concept without proprietary data.
   - Remediation recommendations if available.

### Response Timeline

- **Initial acknowledgment:** Within 48 hours.
- **Triage and preliminary assessment:** Within 5 business days.
- **Patch release & disclosure coordination:** Coordinated once a verified fix has been merged and regression tests pass.
