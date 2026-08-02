[CmdletBinding()]
param(
    [string] $IsccPath,
    [string] $ProjectRoot
)

$ErrorActionPreference = 'Stop'
if ( [string]::IsNullOrWhiteSpace($ProjectRoot) ) {
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
}
if ( [string]::IsNullOrWhiteSpace($IsccPath) ) {
    $Candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    )
    $IsccPath = $Candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if ( -not $IsccPath -or -not (Test-Path -LiteralPath $IsccPath) ) {
    throw 'ISCC.exe is required to validate the installer source.'
}

$Root = Join-Path ([System.IO.Path]::GetTempPath()) "clipcoach-inno-source-$([guid]::NewGuid())"
$FixtureProject = Join-Path $Root 'project'
$SourceRoot = Join-Path $FixtureProject 'release/Release'
$OutputRoot = Join-Path $FixtureProject 'release'
$PortableRoot = Join-Path $Root 'portable'
$PluginRoot = Join-Path $SourceRoot 'clipxtudio'
$FixtureInstallerDirectory = Join-Path $FixtureProject 'installers/windows'
$FixtureBrandingDirectory = Join-Path $FixtureProject 'data/assets/branding'
$Version = (Get-Content -LiteralPath (Join-Path $ProjectRoot 'buildspec.json') -Raw | ConvertFrom-Json).version

try {
    New-Item -ItemType Directory -Path $FixtureInstallerDirectory -Force | Out-Null
    New-Item -ItemType Directory -Path $FixtureBrandingDirectory -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'installers/windows/clipcoach-studio.iss') -Destination $FixtureInstallerDirectory
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'data/assets/branding/clipx-studio.ico') -Destination $FixtureBrandingDirectory
    New-Item -ItemType Directory -Path (Join-Path $PluginRoot 'bin/64bit') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $PluginRoot 'data') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $PluginRoot 'data/qt-plugins/tls') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $PluginRoot 'bin/64bit/clipxtudio.dll') -Value 'installer-layout-fixture' -NoNewline
    Set-Content -LiteralPath (Join-Path $PluginRoot 'data/qt-plugins/tls/qschannelbackend.dll') -Value 'qt-tls-fixture' -NoNewline
    Set-Content -LiteralPath (Join-Path $PluginRoot 'README-INSTALL.txt') -Value 'ClipXtudio installer fixture' -NoNewline
    Copy-Item -Path (Join-Path $ProjectRoot 'data/*') -Destination (Join-Path $PluginRoot 'data') -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'data/license-public.pem.example') -Destination (Join-Path $PluginRoot 'data/license-public.pem')

    $OutputBaseFilename = "clipxtudio-$Version-windows-x64-setup"
    & $IsccPath `
        "/DSourceRoot=$SourceRoot" `
        "/DOutputDir=$OutputRoot" `
        "/DAppVersion=$Version" `
        "/DOutputBaseFilename=$OutputBaseFilename" `
        (Join-Path $FixtureInstallerDirectory 'clipcoach-studio.iss')
    if ( $LASTEXITCODE -ne 0 ) {
        throw 'Inno Setup compilation failed.'
    }

    New-Item -ItemType Directory -Path (Join-Path $PortableRoot 'clipxtudio/bin/64bit') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $PortableRoot 'clipxtudio/data') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $PluginRoot 'bin/64bit/clipxtudio.dll') -Destination (Join-Path $PortableRoot 'clipxtudio/bin/64bit/clipxtudio.dll')
    Copy-Item -Path (Join-Path $PluginRoot 'data/*') -Destination (Join-Path $PortableRoot 'clipxtudio/data') -Recurse -Force
    $PortableZip = Join-Path $OutputRoot "clipxtudio-$Version-windows-x64.zip"
    Compress-Archive -Path (Join-Path $PortableRoot 'clipxtudio') -DestinationPath $PortableZip

    & (Join-Path $ProjectRoot 'tests/packaging/Test-WindowsInstaller.ps1') `
        -InstallerPath (Join-Path $OutputRoot "$OutputBaseFilename.exe") `
        -PortableZip $PortableZip
}
finally {
    if ( Test-Path -LiteralPath $Root ) {
        $ResolvedRoot = (Resolve-Path -LiteralPath $Root).Path
        $ResolvedTemp = (Resolve-Path -LiteralPath ([System.IO.Path]::GetTempPath())).Path
        if ( $ResolvedRoot.StartsWith($ResolvedTemp, [StringComparison]::OrdinalIgnoreCase) ) {
            Remove-Item -LiteralPath $ResolvedRoot -Recurse -Force
        }
    }
}
