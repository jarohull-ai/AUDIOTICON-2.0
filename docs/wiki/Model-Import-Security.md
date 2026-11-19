# Model Import Security

Because machine learning model files can be large and are often sourced from public repositories, Audioticon 2.0 enforces strict security boundaries to prevent malicious payload execution, directory traversal, or silent network connections.

---

## 1. Explicit Network Consent Modal

No network socket is ever opened without explicit user confirmation. When a user clicks **Download…** or enters a remote URL in the Model Hub:

1. **Pre-flight Offline Inspection:**
   `StudioBridge::inspectRemoteCandidate()` parses the candidate reference completely offline and extracts:
   - Remote host and organization
   - Upstream author
   - Stated license
   - Byte size
   - List of file URLs
   - Published checksums
   - Known blockers or system requirements
2. **Glass Consent Dialog:**
   The user is presented with a prominent warning modal:
   - Displays all metadata and file download targets.
   - The **Confirm & Download** button is permanently disabled until the user manually checks:
     > *"I explicitly confirm this source, license, and network download"*
   - For non-curated sources, the user must also supply the expected SHA-256 and byte size.
3. **Cancellation & Rollback:**
   If the user cancels the download, `cancelActiveImport()` halts the worker thread, purges all temporary staging files from `.staging/`, and cleans the state immediately.

---

## 2. Safe Staging Pipeline

All imports (whether local directories, ZIP archives, or remote downloads) follow an atomic staging protocol:

```
Step 1: Create isolated sandbox directory:
        $XDG_DATA_HOME/audioticon/models/.staging/<import-id>/

Step 2: Write intent manifest:
        .staging/<import-id>/intent.json

Step 3: Transfer / copy / unpack files into staging directory.

Step 4: Validate format headers:
        - GGUF files must start with the 'GGUF' magic bytes.
        - Profiles must parse as valid JSON.
        - Executable headers (ELF, MZ, '#!') are immediately rejected.

Step 5: Verify SHA-256 checksum against sidecar or user specification.

Step 6: Atomic directory move to destination:
        $XDG_DATA_HOME/audioticon/models/<kind>/<model-id>/

Step 7: Remove staging directory and clean intent manifest.
```

If any failure occurs during Steps 3–5, the entire staging directory is deleted. The permanent model directory is never left in a half-written or corrupt state.

---

## 3. Defense Against Archive Exploits

When importing `.zip` archives:
- **Zip Slip Defense:** Every entry path is sanitized via `enclosed_name`. Entries containing `..` or absolute paths (`/etc/...`, `C:\...`) are unconditionally rejected.
- **Symlink Defense:** Symbolic links inside archives are ignored and rejected.
- **Executable Denylist:** Files ending in `.exe`, `.dll`, `.so`, `.sh`, `.bat`, `.py`, `.js`, or `.jar` trigger an immediate import failure.

---

*Related Pages:*
* [Model Hub](Model-Hub.md)
* [Security Policy](Security-Policy.md)
* [XDG Paths and Runtime Data](XDG-Paths-and-Runtime-Data.md)
