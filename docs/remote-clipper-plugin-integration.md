# Remote Clipper native plugin integration

## Runtime architecture

The native plugin never exposes OBS or opens a listening port. `RemoteClipperClient` makes authenticated outbound HTTPS requests to ClipXtudio. It receives the bearer token from `LicenseManager` for each request and never persists a second copy.

`RemoteCommandPoller` sends a heartbeat and, only when the backend returns an enabled session, requests claimed commands. The backend-provided interval is clamped to 2–60 seconds (default 3). Only one request cycle can be in flight. Retryable failures use exponential backoff from 3 to 60 seconds. An authorization failure pauses polling until the license observer reports new credentials.

`RemoteCommandExecutor` validates UUIDs, serializes execution, rejects duplicates and uses an explicit allowlist:

- `mark_moment`
- `save_clip_30`
- `save_clip_60`
- `save_custom`
- `save_vertical`
- `save_both`

No remote action can change scenes, start/stop streaming, start/stop Replay Buffer, or modify settings. The local “Allow remote clip commands” toggle is an additional deny switch; the backend remains authoritative for Pro, add-on, device and session state.

## Capture and delay compensation

For a requested duration `D` and delay `L`, the source window must contain `D + L` seconds. The export backend reads from `-(D + L)` relative to the source end and outputs `D` seconds. Therefore the final segment is:

`[now - L - D, now - L]`

Remote sessions prepare OBS Replay Buffer capacity for the maximum supported MVP window (120 seconds duration + 120 seconds delay). A newly enlarged/restarted buffer must fill before commands can succeed; until then the result is `REPLAY_BUFFER_WARMING_UP`.

The original OBS replay is retained as a recovery source. The backend result references the compensated processed file(s). Vertical uses the existing 9:16 center-crop export pipeline; `save_both` queues horizontal and vertical outputs from the same source.

## Lifecycle

Polling starts after licensing, capture, export and settings initialization. It stops on the OBS exit event before sources are destroyed and is destroyed before the capture/export managers. Async network callbacks use guarded QObject pointers so late replies cannot access a destroyed poller.

Completed command UUIDs (maximum 200) are persisted in `remote-clipper-state.ini`. Before local execution, the plugin must successfully acknowledge the device-bound `processing` transition. A failed or ambiguous acknowledgement never executes the command and its claim lease can be recovered by the backend.

Every completed or failed result is written to a durable outbox in the same INI file before upload. Successful acknowledgement removes it; network failure leaves it available for retry and startup replay. The file contains no license token, note, editor email or device fingerprint. Non-retryable failures are logged by error code only.
