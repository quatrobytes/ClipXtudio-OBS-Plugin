# ClipXtudio OBS Plugin architecture

This repository contains only the native OBS plugin and its distributable
runtime assets. ClipXtudio Hub, billing, subscriptions, account management,
license issuance, and hosted AI services are separate private services and are
not part of this repository.

## Native layers

- `src/core`: application rules and models independent from OBS and Qt.
- `src/adapters`: OBS, filesystem, HTTP, media, and secure-storage adapters.
- `src/ui`: the OBS dock, dialogs, translations, and view models.
- `include`: public headers shared by native targets.
- `tests`: native unit, contract, and UI tests.

Dependencies point inward: UI and adapters use the core contracts; the core
does not depend on OBS, Qt, HTTP, or the hosted service implementation.

## Hosted-service boundary

Optional online features use outbound HTTPS requests. The plugin never opens a
listening port and never exposes OBS WebSocket. Authentication material is
stored through the platform secure-storage implementation, is never committed,
and must not be logged.

The hosted service remains authoritative for entitlements and remote commands.
The plugin accepts only its documented command allowlist and cannot remotely
change scenes, start or stop a stream, or modify account and billing settings.

## Media boundary

Capture and replay operations execute locally through OBS. Editing and export
also remain local; video, audio, and filesystem paths are not uploaded by
default. Features that intentionally send text to a hosted service require the
corresponding consent and entitlement.

## Packaging

Official release installers are generated from a version tag. Release jobs are
configured to require Windows code signing. Ordinary branch builds validate the
source without publishing an unsigned installer.
