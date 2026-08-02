[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $InstallerPath,
    [Parameter(Mandatory)]
    [string] $PortableZip
)

$ErrorActionPreference = 'Stop'

function Assert-True {
    param([bool] $Condition, [string] $Message)
    if ( -not $Condition ) {
        throw $Message
    }
}

function Invoke-ProcessAndWait {
    param(
        [Parameter(Mandatory)]
        [string] $FilePath,
        [string[]] $ArgumentList = @()
    )

    # Start-Process -Wait can keep waiting for helper descendants spawned by an
    # Inno Setup uninstaller even after the actual installer has exited.
    $Process = Start-Process -FilePath $FilePath -ArgumentList $ArgumentList -PassThru
    $Process.WaitForExit()
    return $Process
}

$Installer = (Resolve-Path -LiteralPath $InstallerPath).Path
$Zip = (Resolve-Path -LiteralPath $PortableZip).Path
$Root = Join-Path ([System.IO.Path]::GetTempPath()) "clipcoach-installer-test-$([guid]::NewGuid())"
$PluginRoot = Join-Path $Root 'clipxtudio'
$ExtractRoot = Join-Path $Root 'portable'
$LogPath = Join-Path $Root 'install.log'
$PreviousAppData = $env:APPDATA

try {
    New-Item -ItemType Directory -Path $PluginRoot -Force | Out-Null
    $env:APPDATA = Join-Path $Root 'appdata'

    $InstallArguments = @(
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART',
        '/CURRENTUSER',
        "/DIR=$PluginRoot",
        "/LOG=$LogPath"
    )
    $Install = Invoke-ProcessAndWait -FilePath $Installer -ArgumentList $InstallArguments
    Assert-True ($Install.ExitCode -eq 0) "Installer failed with exit code $($Install.ExitCode)."

    $Plugin = Join-Path $PluginRoot 'bin/64bit/clipxtudio.dll'
    $Locale = Join-Path $PluginRoot 'data/locale/en-US.ini'
    $SpanishLocale = Join-Path $PluginRoot 'data/locale/es-ES.ini'
    $Asset = Join-Path $PluginRoot 'data/assets/manifest.json'
    $PublicKey = Join-Path $PluginRoot 'data/license-public.pem'
    Assert-True (Test-Path -LiteralPath $Plugin) 'Plugin DLL was not installed in the OBS plugin directory.'
    Assert-True (Test-Path -LiteralPath $Locale) 'Locale files were not installed.'
    Assert-True (Test-Path -LiteralPath $SpanishLocale) 'Spanish locale was not installed.'
    Assert-True ([bool] (Select-String -LiteralPath $Locale -SimpleMatch 'Settings.Section.RemoteClipper=' -Quiet)) 'English locale is stale or missing Remote Clipper translations.'
    Assert-True ([bool] (Select-String -LiteralPath $SpanishLocale -SimpleMatch 'Settings.Section.RemoteClipper=' -Quiet)) 'Spanish locale is stale or missing Remote Clipper translations.'
    Assert-True (Test-Path -LiteralPath $Asset) 'Plugin assets were not installed.'
    Assert-True (Test-Path -LiteralPath $PublicKey) 'License public key was not installed.'

    $FirstHash = (Get-FileHash -LiteralPath $Plugin -Algorithm SHA256).Hash
    $Upgrade = Invoke-ProcessAndWait -FilePath $Installer -ArgumentList $InstallArguments
    Assert-True ($Upgrade.ExitCode -eq 0) 'Installing the same version as an upgrade failed.'
    Assert-True ((Get-FileHash -LiteralPath $Plugin -Algorithm SHA256).Hash -eq $FirstHash) 'Upgrade changed the packaged DLL unexpectedly.'

    $PreservedData = Join-Path $PluginRoot 'data/user-created.keep'
    Set-Content -LiteralPath $PreservedData -Value 'must survive uninstall' -NoNewline
    $Uninstaller = Get-ChildItem -LiteralPath $PluginRoot -Filter 'unins*.exe' | Select-Object -First 1
    Assert-True ($null -ne $Uninstaller) 'Uninstaller was not created.'
    $Uninstall = Invoke-ProcessAndWait -FilePath $Uninstaller.FullName -ArgumentList @(
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART'
    )
    Assert-True ($Uninstall.ExitCode -eq 0) 'Uninstaller failed.'
    Assert-True (-not (Test-Path -LiteralPath $Plugin)) 'Uninstaller left the plugin DLL behind.'
    Assert-True (Test-Path -LiteralPath $PreservedData) 'Uninstaller deleted user-created data.'

    Expand-Archive -LiteralPath $Zip -DestinationPath $ExtractRoot
    Assert-True (Test-Path -LiteralPath (Join-Path $ExtractRoot 'clipxtudio/bin/64bit/clipxtudio.dll')) 'Portable ZIP has an invalid plugin layout.'
    Assert-True (Test-Path -LiteralPath (Join-Path $ExtractRoot 'clipxtudio/data/locale/es-ES.ini')) 'Portable ZIP is missing locales.'

    Write-Output 'Windows installer, upgrade, uninstall and portable layout tests passed.'
}
finally {
    $env:APPDATA = $PreviousAppData
    if ( Test-Path -LiteralPath $Root ) {
        Remove-Item -LiteralPath $Root -Recurse -Force
    }
}
