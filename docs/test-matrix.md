# Native plugin test matrix

| Area | Automated | Manual OBS | Release blocker |
|---|---:|---:|---:|
| Core capture rules | Yes | Yes | Yes |
| Settings import/export | Yes | Yes | Yes |
| Replay Buffer state | Partial | Yes | Yes |
| Vertical processing | Partial | Yes | Yes |
| Quick editor/export | Partial | Yes | Yes |
| Voice trigger | Partial | Yes | Yes |
| Remote command allowlist | Yes | Yes | Yes |
| Authentication failures | Yes | Yes | Yes |
| Network backoff/outbox | Yes | Yes | Yes |
| UI localization/layout | Partial | Yes | Yes |
| Installer/signature | Metadata | Yes | Yes |

"Partial" means platform or OBS behavior still requires an end-to-end run; it
must not be reported as fully passing from a core-only test.
