$ErrorActionPreference = 'Stop'

$mavenVersion = '3.9.16'
$mavenArchiveName = "apache-maven-$mavenVersion-bin.zip"
$mavenBaseUrl = "https://dlcdn.apache.org/maven/maven-3/$mavenVersion/binaries"
$mavenArchiveUrl = "$mavenBaseUrl/$mavenArchiveName"
$mavenChecksumUrl = "https://downloads.apache.org/maven/maven-3/$mavenVersion/binaries/$mavenArchiveName.sha512"

$installRoot = Join-Path $env:LOCALAPPDATA 'Programs\Apache\Maven'
$mavenHome = Join-Path $installRoot "apache-maven-$mavenVersion"
$downloadRoot = Join-Path $env:TEMP "maven-install-$mavenVersion"
$archivePath = Join-Path $downloadRoot $mavenArchiveName
$checksumPath = "$archivePath.sha512"
$settingsPath = Join-Path $env:USERPROFILE '.m2\settings.xml'

function Normalize-PathEntry {
    param([Parameter(Mandatory)][string]$Entry)

    $expanded = [Environment]::ExpandEnvironmentVariables($Entry.Trim())
    try {
        return [IO.Path]::GetFullPath($expanded).TrimEnd('\').ToLowerInvariant()
    }
    catch {
        return $expanded.TrimEnd('\').ToLowerInvariant()
    }
}

Write-Host "Installing Apache Maven $mavenVersion..."

New-Item -ItemType Directory -Force -Path $downloadRoot, $installRoot | Out-Null

Invoke-WebRequest -Uri $mavenArchiveUrl -OutFile $archivePath
Invoke-WebRequest -Uri $mavenChecksumUrl -OutFile $checksumPath

$expectedHash = ((Get-Content -LiteralPath $checksumPath -Raw).Trim() -split '\s+')[0]
$actualHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA512).Hash
if ($actualHash -ne $expectedHash) {
    throw "Maven archive SHA-512 verification failed."
}

if (-not (Test-Path -LiteralPath $mavenHome)) {
    Expand-Archive -LiteralPath $archivePath -DestinationPath $installRoot
}

[Environment]::SetEnvironmentVariable('MAVEN_HOME', $mavenHome, 'User')

$javaHome = [Environment]::GetEnvironmentVariable('JAVA_HOME', 'User')
if ([string]::IsNullOrWhiteSpace($javaHome)) {
    throw 'User-level JAVA_HOME is not configured.'
}

$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
$existingEntries = @(
    $userPath -split ';' |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
)

$javaBinNormalized = Normalize-PathEntry (Join-Path $javaHome 'bin')
$mavenBinNormalized = Normalize-PathEntry (Join-Path $mavenHome 'bin')
$seen = @{}
$cleanEntries = foreach ($entry in $existingEntries) {
    $normalized = Normalize-PathEntry $entry

    if ($normalized -eq $javaBinNormalized -or $normalized -eq $mavenBinNormalized) {
        continue
    }

    if (-not $seen.ContainsKey($normalized)) {
        $seen[$normalized] = $true
        $entry.Trim()
    }
}

$newUserPath = (@($cleanEntries) + '%JAVA_HOME%\bin' + '%MAVEN_HOME%\bin') -join ';'
[Environment]::SetEnvironmentVariable('Path', $newUserPath, 'User')

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $settingsPath) | Out-Null

$settingsXml = @'
<?xml version="1.0" encoding="UTF-8"?>
<settings xmlns="http://maven.apache.org/SETTINGS/1.2.0"
          xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
          xsi:schemaLocation="http://maven.apache.org/SETTINGS/1.2.0 https://maven.apache.org/xsd/settings-1.2.0.xsd">
  <mirrors>
    <mirror>
      <id>aliyun-public</id>
      <name>Aliyun Maven Public Mirror</name>
      <url>https://maven.aliyun.com/repository/public</url>
      <mirrorOf>central</mirrorOf>
    </mirror>
  </mirrors>

  <profiles>
    <profile>
      <id>jdk-21</id>
      <activation>
        <activeByDefault>true</activeByDefault>
        <jdk>21</jdk>
      </activation>
      <properties>
        <maven.compiler.release>21</maven.compiler.release>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <project.reporting.outputEncoding>UTF-8</project.reporting.outputEncoding>
      </properties>
    </profile>
  </profiles>
</settings>
'@

Set-Content -LiteralPath $settingsPath -Value $settingsXml -Encoding utf8

# Refresh this PowerShell process so verification works immediately.
$env:MAVEN_HOME = $mavenHome
$env:Path = "$javaHome\bin;$mavenHome\bin;" + $env:Path

Write-Host ''
Write-Host 'Maven installation completed.'
Write-Host "MAVEN_HOME: $mavenHome"
Write-Host "Settings:   $settingsPath"
Write-Host ''

mvn -version
mvn help:effective-settings -Doutput="$downloadRoot\effective-settings.xml" --quiet
Write-Host "Effective settings verified: $downloadRoot\effective-settings.xml"
