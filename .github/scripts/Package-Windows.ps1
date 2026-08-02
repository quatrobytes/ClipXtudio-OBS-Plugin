[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'Release',
    [switch] $SkipInstallerTests,
    [switch] $AllowMissingPublicKey,
    [switch] $RequireSigning
)

$ErrorActionPreference = 'Stop'
if ( $env:GITHUB_REF_TYPE -eq 'tag' ) {
    $RequireSigning = $true
}

function Find-InnoCompiler {
    $Command = Get-Command 'ISCC.exe' -ErrorAction SilentlyContinue
    if ( $Command ) {
        return $Command.Source
    }
    $KnownPaths = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    )
    foreach ( $KnownPath in $KnownPaths ) {
        if ( Test-Path -LiteralPath $KnownPath ) {
            return $KnownPath
        }
    }
    throw 'Inno Setup 6 was not found. Install it or add ISCC.exe to PATH.'
}

function Find-SignTool {
    $Command = Get-Command 'signtool.exe' -ErrorAction SilentlyContinue
    if ( $Command ) {
        return $Command.Source
    }
    $Candidates = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending
    if ( $Candidates ) {
        return $Candidates[0].FullName
    }
    return $null
}

function Invoke-AuthenticodeSign {
    param(
        [Parameter(Mandatory)]
        [string] $File,
        [Parameter(Mandatory)]
        [string] $TemporaryDirectory
    )

    if ( -not $env:WINDOWS_SIGNING_CERT_BASE64 -and -not $env:WINDOWS_SIGNING_CERT_SHA1 ) {
        if ( $RequireSigning ) {
            throw 'Authenticode signing credentials are required for tagged/public releases.'
        }
        Write-Warning "Signing credentials are not configured; leaving $(Split-Path $File -Leaf) unsigned."
        return
    }

    $SignTool = Find-SignTool
    if ( -not $SignTool ) {
        throw 'Signing was requested but signtool.exe was not found.'
    }

    $Arguments = @('sign', '/fd', 'sha256', '/td', 'sha256', '/tr', 'http://timestamp.digicert.com')
    if ( $env:WINDOWS_SIGNING_CERT_BASE64 ) {
        $CertificatePath = Join-Path $TemporaryDirectory 'windows-signing.pfx'
        [IO.File]::WriteAllBytes($CertificatePath, [Convert]::FromBase64String($env:WINDOWS_SIGNING_CERT_BASE64))
        $Arguments += @('/f', $CertificatePath)
        if ( $env:WINDOWS_SIGNING_CERT_PASSWORD ) {
            $Arguments += @('/p', $env:WINDOWS_SIGNING_CERT_PASSWORD)
        }
    }
    else {
        $Arguments += @('/sha1', $env:WINDOWS_SIGNING_CERT_SHA1)
    }
    $Arguments += $File

    & $SignTool @Arguments
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Authenticode signing failed for $File."
    }
    & $SignTool verify /pa /v $File
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Authenticode verification failed for $File."
    }
}

$ProjectRoot = (Resolve-Path "$PSScriptRoot/../..").Path
$BuildSpec = Get-Content -LiteralPath (Join-Path $ProjectRoot 'buildspec.json') -Raw | ConvertFrom-Json
$ProductName = [string] $BuildSpec.name
$ProductVersion = [string] $BuildSpec.version
$InstallRoot = Join-Path $ProjectRoot "release/$Configuration"
$InstalledPluginRoot = Join-Path $InstallRoot $ProductName
$PluginBinary = Join-Path $InstalledPluginRoot "bin/64bit/$ProductName.dll"
$BuiltPluginBinary = Join-Path $ProjectRoot "build_x64/$Configuration/$ProductName.dll"
$BuiltPluginData = Join-Path $ProjectRoot "build_x64/rundir/$Configuration/$ProductName"
$PluginData = Join-Path $InstalledPluginRoot 'data'
$InstallerReadme = Join-Path $ProjectRoot 'installers/windows/README-INSTALL.txt'
$PublicKey = Join-Path $PluginData 'license-public.pem'
$ReleaseRoot = Join-Path $ProjectRoot 'release'
$OutputBase = "$ProductName-$ProductVersion-windows-$Target"
$PortableZip = Join-Path $ReleaseRoot "$OutputBase.zip"
$Installer = Join-Path $ReleaseRoot "$OutputBase-setup.exe"
$StagingRoot = Join-Path $ReleaseRoot ".package-$([guid]::NewGuid())"

& (Join-Path $ProjectRoot 'tests/packaging/Test-ReleaseMetadata.ps1') -ProjectRoot $ProjectRoot

if ( -not (Test-Path -LiteralPath $BuiltPluginBinary) ) {
    throw "Current build plugin binary is missing: $BuiltPluginBinary"
}
if ( -not (Test-Path -LiteralPath (Join-Path $BuiltPluginData 'locale/en-US.ini')) ) {
    throw "Current build plugin data is missing: $BuiltPluginData"
}

New-Item -ItemType Directory -Path (Split-Path -Parent $PluginBinary) -Force | Out-Null
New-Item -ItemType Directory -Path $PluginData -Force | Out-Null
Copy-Item -LiteralPath $BuiltPluginBinary -Destination $PluginBinary -Force
# CMake stages the authoritative runtime resources under rundir. Always refresh
# the installer tree from that output so a new DLL cannot be packaged with stale
# translations, assets, models or helper binaries from a previous release.
Copy-Item -Path (Join-Path $BuiltPluginData '*') -Destination $PluginData -Recurse -Force
Copy-Item -LiteralPath $InstallerReadme -Destination (Join-Path $InstalledPluginRoot 'README-INSTALL.txt') -Force
# Qt Multimedia is intentionally not shipped. OBS owns the Qt runtime and a
# plugin-side Qt Multimedia DLL from another minor release prevents OBS from
# loading the complete module. The quick editor decodes previews with the
# bundled FFmpeg executable instead.
Remove-Item -LiteralPath `
    (Join-Path (Split-Path -Parent $PluginBinary) 'Qt6Multimedia.dll'), `
    (Join-Path (Split-Path -Parent $PluginBinary) 'Qt6MultimediaWidgets.dll'), `
    (Join-Path $PluginData 'qt-plugins/multimedia') `
    -Recurse -Force -ErrorAction SilentlyContinue

$BinaryVersion = (Get-Item -LiteralPath $PluginBinary).VersionInfo.ProductVersion
if ( $BinaryVersion -ne $ProductVersion ) {
    throw "Plugin binary version $BinaryVersion does not match release version $ProductVersion."
}

if ( -not (Test-Path -LiteralPath $PluginBinary) ) {
    throw "Release plugin binary is missing: $PluginBinary"
}
if ( -not (Test-Path -LiteralPath (Join-Path $PluginData 'locale/en-US.ini')) ) {
    throw "Plugin data files are missing from $PluginData"
}
if ( -not (Test-Path -LiteralPath $PublicKey) -and -not $AllowMissingPublicKey ) {
    throw 'data/license-public.pem is required for a distributable Pro build. Inject LICENSE_SIGNING_PUBLIC_KEY_B64 before configuring CMake.'
}

try {
    $PortableRoot = Join-Path $StagingRoot $ProductName
    New-Item -ItemType Directory -Path (Join-Path $PortableRoot 'bin/64bit') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $PortableRoot 'data') -Force | Out-Null
    Copy-Item -Path (Join-Path (Split-Path -Parent $PluginBinary) '*.dll') `
        -Destination (Join-Path $PortableRoot 'bin/64bit')
    Copy-Item -Path (Join-Path $PluginData '*') -Destination (Join-Path $PortableRoot 'data') -Recurse -Force
    Copy-Item `
        -LiteralPath (Join-Path $InstalledPluginRoot 'README-INSTALL.txt') `
        -Destination (Join-Path $PortableRoot 'README-INSTALL.txt')

    Invoke-AuthenticodeSign -File $PluginBinary -TemporaryDirectory $StagingRoot
    Copy-Item -Path (Join-Path (Split-Path -Parent $PluginBinary) '*.dll') `
        -Destination (Join-Path $PortableRoot 'bin/64bit') -Force

    Remove-Item -LiteralPath $PortableZip, $Installer -Force -ErrorAction SilentlyContinue
    Compress-Archive -Path $PortableRoot -DestinationPath $PortableZip -CompressionLevel Optimal

    $InnoCompiler = Find-InnoCompiler
    $InnoScript = Join-Path $ProjectRoot 'installers/windows/clipcoach-studio.iss'
    & $InnoCompiler `
        "/DSourceRoot=$InstallRoot" `
        "/DOutputDir=$ReleaseRoot" `
        "/DAppVersion=$ProductVersion" `
        "/DOutputBaseFilename=$OutputBase-setup" `
        $InnoScript
    if ( $LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $Installer) ) {
        throw 'Inno Setup did not produce the expected installer.'
    }

    Invoke-AuthenticodeSign -File $Installer -TemporaryDirectory $StagingRoot

    if ( -not $SkipInstallerTests ) {
        & (Join-Path $ProjectRoot 'tests/packaging/Test-WindowsInstaller.ps1') `
            -InstallerPath $Installer `
            -PortableZip $PortableZip
    }

    Get-FileHash -Algorithm SHA256 -LiteralPath $Installer, $PortableZip |
        ForEach-Object { "$($_.Hash.ToLowerInvariant())  $(Split-Path $_.Path -Leaf)" } |
        Set-Content -LiteralPath (Join-Path $ReleaseRoot "$OutputBase-SHA256SUMS.txt") -Encoding ascii

    Write-Output "Created $Installer"
    Write-Output "Created $PortableZip"
}
finally {
    if ( Test-Path -LiteralPath $StagingRoot ) {
        $ResolvedStaging = (Resolve-Path -LiteralPath $StagingRoot).Path
        if ( $ResolvedStaging.StartsWith($ReleaseRoot, [StringComparison]::OrdinalIgnoreCase) ) {
            Remove-Item -LiteralPath $ResolvedStaging -Recurse -Force
        }
    }
}
