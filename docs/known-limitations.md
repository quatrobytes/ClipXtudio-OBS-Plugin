# Known limitations

## Remote Clipper MVP

- Polling is used instead of realtime WebSocket delivery. Normal command pickup latency is 2–5 seconds plus network latency.
- OBS Replay Buffer must already be active and contain the full `duration + delay` history. A buffer that has just started or been enlarged returns `REPLAY_BUFFER_WARMING_UP`; missing history cannot be reconstructed.
- Exact delay compensation creates a processed copy and retains the original OBS replay as a recovery source. This temporarily uses additional disk space.
- Vertical remote clips currently use the production center-crop export path. A future canvas renderer can supply the configured vertical scene directly without changing the remote API.
- `mark_moment` records an immediate local session marker and backend command timeline entry; it does not create a media file.
- Result delivery retries transient failures five times in memory. If OBS terminates before the backend acknowledges the result, the backend command can eventually expire; durable offline result outbox is planned after MVP.
- One capture is processed at a time because OBS exposes a single Replay Buffer save pipeline. Additional claimed commands queue locally and preserve order.
- The native OBS Replay Buffer records the current program output. The scene selected under Vertical configures ClipXtudio's preview and managed OBS scene, but it is not recorded as a second historical video stream. Vertical and Both therefore create processed 9:16 copies from the saved OBS program replay until a dedicated vertical replay renderer is introduced.
