# Security Policy

Please refer to the repository's root [SECURITY.md](https://github.com/jarohull-ai/AUDIOTICON-2.0/blob/main/SECURITY.md) for vulnerability reporting procedures and threat model details.

---

## Security Highlights

- **Offline-First:** Audioticon 2.0 does not listen on any network ports.
- **Explicit Consent:** Remote model downloads require explicit user approval in a dedicated modal showing byte size, author, license, and checksums.
- **Archive Safety:** ZIP archive extraction strictly validates paths against directory traversal (`..`) and ignores executable scripts.
- **Memory Safety:** The domain logic is written in Rust and interfaced to C++ through `cxx`.

---

## Reporting Vulnerabilities

To report a vulnerability confidentially:
- Submit via [GitHub Private Vulnerability Reporting](https://github.com/jarohull-ai/AUDIOTICON-2.0/security/advisories/new).
- Or email: `security@jarohull.ai`.

---

*Related Pages:*
* [Model Import Security](Model-Import-Security.md)
* [XDG Paths and Runtime Data](XDG-Paths-and-Runtime-Data.md)
