param([string]$Compiler = '')
$ErrorActionPreference='Stop'
if (!$Compiler) { $found=Get-Command g++ -ErrorAction SilentlyContinue; if ($found) {$Compiler=$found.Source} elseif (Test-Path 'D:\QT\Tools\mingw1310_64\bin\g++.exe') {$Compiler='D:\QT\Tools\mingw1310_64\bin\g++.exe'} else {throw '请安装 MinGW-w64，或使用 -Compiler 指定 g++.exe。'} }
$env:PATH=(Split-Path $Compiler)+';'+$env:PATH
Push-Location $PSScriptRoot
try {
 & $Compiler '-std=c++17' '-O2' '-Wall' '-Wextra' '-Wno-missing-field-initializers' '-municode' '-mwindows' '-static' '-finput-charset=UTF-8' 'src/main.cpp' '-o' 'SpeechNotes.exe' '-lsapi' '-lole32' '-luuid' '-lcomctl32' '-lcomdlg32' '-lshell32'
 if ($LASTEXITCODE -ne 0) {throw '编译失败'}
 Write-Host '编译成功：SpeechNotes.exe'
} finally {Pop-Location}

