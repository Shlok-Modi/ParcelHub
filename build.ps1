# Builds the demo and all tests with g++ and runs the tests. Works in Windows PowerShell 5+.
# Usage (from the folder containing "algorithms" and "tests"):  .\build.ps1
$ErrorActionPreference = "Stop"
$core = Get-ChildItem algorithms/src -Filter *.cpp | Where-Object { $_.Name -ne "main.cpp" } | ForEach-Object { $_.FullName }
New-Item -ItemType Directory -Force build | Out-Null
$flags = @("-std=c++17", "-O2", "-Wall", "-Wextra", "-Ialgorithms/include", "-Itests")

Write-Host "Compiling demo..."
& g++ @flags @core algorithms/src/main.cpp -o build/parcelhub.exe
if ($LASTEXITCODE -ne 0) { throw "Compile failed: parcelhub" }

$failed = 0
foreach ($t in "heapTests", "crossHeapTests", "routingTests", "primTests", "dispatchTests") {
    Write-Host "Compiling and running $t..."
    & g++ @flags @core "tests/$t.cpp" -o "build/$t.exe"
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $t" }
    & "build/$t.exe"
    if ($LASTEXITCODE -ne 0) { $failed++ }
}
if ($failed -gt 0) { Write-Host "$failed test suite(s) FAILED" -ForegroundColor Red; exit 1 }
Write-Host "All test suites passed. Run the demo with:  .\build\parcelhub.exe [binary|binomial|fibonacci]" -ForegroundColor Green
