# Plugin settings

## First-run setup assistant

ClipXtudio opens a five-step assistant the first time its OBS dock is loaded for
each installed plugin version. It configures the Free workflow first:

1. dock startup and in-OBS notifications;
2. default clip format, duration and clips folder;
3. local voice recognition, OBS microphone, language and phrases;
4. vertical OBS scene and resolution;
5. completion plus optional Pro license activation or pricing link.

Completing the assistant persists `initial_setup_completed` and
`initial_setup_completed_version`. It does not start Replay Buffer. Reopening
OBS with the same plugin version does not show the assistant again. A newer
plugin version can show its revised assistant once. Users can always reopen it
from **Settings > Settings profile > Open setup assistant**.

## Portable settings profile

Settings profile export/import is available on Free and Pro. It transfers the
complete ClipXtudio workflow configuration as validated JSON, including voice
phrases, triggers, capture defaults and vertical composition preferences. It
does not include license keys, authorization tokens, device credentials or OBS
scene collections/media.

## Remote Clipper

The Account/Settings area contains a Remote Clipper card with:

- local allow/pause toggle;
- connection state;
- active session identifier;
- last successful heartbeat;
- backend polling interval.

The header shows `Remote: connected`, `offline`, `paused`, `connecting`, or `authorization required`, and displays the pending command count while a batch is delivered.

`remote_commands_enabled` was added in settings schema 13 and defaults to `true`. Settings schema 14 adds the completed assistant version. Migrating an older settings file preserves all existing values and adds these defaults. Disabling Remote Clipper does not cancel the subscription or remote session; it only prevents this OBS installation from fetching/executing commands.

The toggle does not start Replay Buffer. The streamer remains responsible for starting Replay Buffer in OBS/ClipXtudio.
