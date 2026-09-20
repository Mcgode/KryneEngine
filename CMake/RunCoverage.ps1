# Runs the unit tests and builds an LLVM source-based coverage report.
# Windows only (native Clang/clang-cl, non-cross-compiled) — see RunCoverage.sh for macOS/Linux.
# Requires configuring CMake with -DKRYNE_ENGINE_ENABLE_COVERAGE=ON and building the Tests target.
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$Compiler
)

$ErrorActionPreference = "Stop"

$CoverageDir = Join-Path $BuildDir "coverage"
$RawDir = Join-Path $CoverageDir "raw"

# Resolve llvm-cov/llvm-profdata through the compiler driver rather than searching PATH: this
# guarantees the tool matches the Clang version that produced the instrumentation, which the
# .profraw/.profdata formats are sensitive to.
$LlvmProfdata = (& $Compiler -print-prog-name=llvm-profdata).Trim()
$LlvmCov = (& $Compiler -print-prog-name=llvm-cov).Trim()

if (Test-Path $CoverageDir) { Remove-Item -Recurse -Force $CoverageDir }
New-Item -ItemType Directory -Force -Path $RawDir | Out-Null

Write-Host "Running tests with coverage instrumentation..."
$env:LLVM_PROFILE_FILE = Join-Path $RawDir "%p.profraw"
ctest --test-dir $BuildDir --output-on-failure
Remove-Item Env:\LLVM_PROFILE_FILE

$TestBinaries = @(Get-ChildItem -Path (Join-Path $BuildDir "Tests") -Recurse -Filter "*_UnitTests.exe" | ForEach-Object { $_.FullName })
if ($TestBinaries.Count -eq 0) {
    Write-Error "No test binaries found under $BuildDir\Tests."
    exit 1
}

& $LlvmProfdata merge -sparse (Join-Path $RawDir "*.profraw") -o (Join-Path $CoverageDir "coverage.profdata")

$ObjectArgs = @($TestBinaries[0])
foreach ($bin in $TestBinaries[1..($TestBinaries.Count - 1)]) {
    $ObjectArgs += "-object"
    $ObjectArgs += $bin
}

# Only report on KryneEngine's own sources, not tests, third-party code, or generated files.
$IgnoreRegex = '(^|[\\/])(External|Tests|_deps)[\\/]'
$ProfdataPath = Join-Path $CoverageDir "coverage.profdata"

& $LlvmCov report @ObjectArgs "-instr-profile=$ProfdataPath" "-ignore-filename-regex=$IgnoreRegex"

$HtmlDir = Join-Path $CoverageDir "html"
& $LlvmCov show @ObjectArgs "-instr-profile=$ProfdataPath" "-ignore-filename-regex=$IgnoreRegex" -format=html "-output-dir=$HtmlDir"

Write-Host "HTML report: $(Join-Path $HtmlDir 'index.html')"
