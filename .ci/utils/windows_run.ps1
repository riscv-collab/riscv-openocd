$ErrorActionPreference = "Stop"

$ID = "$env:DISTRIBUTION_ID"
$URL = "http://artifactory.dev.syntacore.com/artifactory/tools-gitlab-artifacts/openocd/${ID}/windows_openocd.zip"

echo "Downloading from: ${URL}"

Invoke-WebRequest -Uri $URL `
  -OutFile "windows_openocd.zip" `

echo "Extracting archive..."
Expand-Archive windows_openocd.zip -DestinationPath . -Force

echo "Running openocd.exe --version"

# WTF: https://mnaoumov.wordpress.com/2015/01/11/execution-of-external-commands-in-powershell-done-right/
#
# https://stackoverflow.com/questions/2095088/error-when-calling-3rd-party-executable-from-powershell-when-using-an-ide
#
# https://stackoverflow.com/a/63490281
$output = $(cmd /c "$(pwd)/openocd/bin/openocd.exe --version 2>&1")
echo "$output"
