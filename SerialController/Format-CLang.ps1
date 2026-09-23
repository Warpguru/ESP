# Format-CLang.ps1 - Reformat all C/C++ sources to Arduino IDE style (2-space indent).
# Uses the clang-format bundled with Arduino IDE.
# Run from the workspace root: .\SerialController\Format-CLang.ps1

$cf = "d:\development\Arduino\Users\ArduinoIDE\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\clang-format.exe"

if (-not (Test-Path $cf)) {
  Write-Error "clang-format not found at: $cf"
  Write-Error "Update the path in this script to match your Arduino IDE installation."
  exit 1
}

$files = Get-ChildItem -Path "$PSScriptRoot" -Recurse -Include "*.cpp","*.h"
$count = 0
foreach ($f in $files) {
  & $cf -i $f.FullName
  $count++
}
Write-Host "Formatted $count file(s)."
