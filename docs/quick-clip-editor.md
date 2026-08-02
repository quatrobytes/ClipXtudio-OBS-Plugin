# Quick Clip Editor

## Timeline editing

The editor renders a local timeline with time ticks, a filmstrip, the audio
waveform, the current playhead and the main in/out handles. The playhead can be
dragged or moved by clicking the timeline. Space toggles playback. Selection is
only an editing state: playback starts at the blue playhead and continues across
all following retained blocks, automatically skipping removed ranges.

Manual cuts use a non-destructive split workflow:

1. Move the playhead to the beginning of an unwanted range.
2. Select **Split** or press `Ctrl+B`.
3. Repeat at the end of the unwanted range.
4. Click the resulting block and select **Delete cut** or press `Delete`.
   Hold `Ctrl` while clicking to select multiple non-adjacent blocks.
5. Review the joined remaining blocks and export.

The source file is never modified. Export builds the complement of all removed
ranges, trims each retained audio/video segment with FFmpeg, resets timestamps,
and concatenates the segments into one MP4. Vertical library items are encoded
to an exact 9:16 frame, including when their OBS replay source used a landscape
canvas. **Undo cuts** clears the edit list.

## Smart trim

Smart trim is local and deterministic in the current release. It analyzes the
audio track with FFmpeg silence detection (`-50 dB`, minimum `0.65 s`) and splits
silent ranges into orange candidate blocks, keeping a 100 ms boundary so spoken
words are not cut at the edge. Candidates start selected, but are not deleted
until the user confirms the contextual trash action or presses `Delete`. The
button says **Delete cut** for one block and **Delete selected (N)** for multiple
blocks, regardless of whether the selection came from Smart Trim or a manual
split. `Ctrl+click` removes individual candidates from that selection. No clip
or audio is uploaded for analysis.

This is silence removal, not semantic scene detection. Music, background noise,
or a noisy microphone can keep a range that a human editor might remove.

## Commercial model

Do not create a separate Stripe charge for every $0.99 operation. Processing
fees and retries make that model inefficient. The recommended paid version is a
prepaid credit wallet:

- one smart cloud trim consumes one credit;
- the displayed reference price can be $0.99 per credit;
- credits are purchased in packs;
- the backend reserves a credit when processing starts;
- it commits the credit only after a valid export is produced;
- failed/cancelled jobs release the reservation;
- Stripe and the credit ledger remain the sources of truth.

The local silence-removal implementation in this release does not consume or
pretend to consume credits. Charging must not be enabled until the backend has
an idempotent reserve/commit/release ledger and webhook reconciliation.
