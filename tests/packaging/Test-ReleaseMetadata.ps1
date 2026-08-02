[CmdletBinding()]
param([string] $ProjectRoot)

$ErrorActionPreference = 'Stop'
if ( [string]::IsNullOrWhiteSpace($ProjectRoot) ) {
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
}
$BuildSpec = Get-Content -LiteralPath (Join-Path $ProjectRoot 'buildspec.json') -Raw | ConvertFrom-Json
$Version = [string] $BuildSpec.version

if ( $Version -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$' ) {
    throw "buildspec.json version '$Version' is not semantic MAJOR.MINOR.PATCH."
}

$Changelog = Get-Content -LiteralPath (Join-Path $ProjectRoot 'CHANGELOG.md') -Raw
if ( $Changelog -notmatch [regex]::Escape("## [$Version]") ) {
    throw "CHANGELOG.md has no section for $Version."
}

$Notes = Join-Path $ProjectRoot "docs/release-notes/$Version.md"
if ( -not (Test-Path -LiteralPath $Notes) ) {
    throw "Release notes are missing: $Notes"
}

$InstallerSource = Get-Content -LiteralPath (Join-Path $ProjectRoot 'installers/windows/clipcoach-studio.iss') -Raw
$RequiredInstallerRules = @(
    'DefaultDirName={commonappdata}\obs-studio\plugins\clipxtudio',
    'PrivilegesRequired=admin',
    'bin\64bit\clipxtudio.dll',
    'data\locale',
    'data\assets',
    'data\tools',
    'SetupIconFile={#SetupIcon}'
)
foreach ( $Rule in $RequiredInstallerRules ) {
    if ( -not $InstallerSource.Contains($Rule) ) {
        throw "Windows installer is missing required rule: $Rule"
    }
}
if ( $InstallerSource -match '(?im)^\s*\[UninstallDelete\]' ) {
    throw 'Recursive uninstall deletion rules are forbidden because they can erase user data.'
}
if ( $InstallerSource -notmatch [regex]::Escape("#define AppVersion `"$Version`"") ) {
    throw "Direct Inno Setup version does not match buildspec.json version $Version."
}

Write-Output "Release metadata $Version is consistent."
