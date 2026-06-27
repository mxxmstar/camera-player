# Clean PATH of garbage entries
$cleanPath = @()
$env:PATH -split ';' | ForEach-Object {
    if ($_ -and $_ -notmatch 'SAFE_RM' -and $_ -notmatch '\.git$' -and $_ -notmatch '\.vscode$' -and $_ -notmatch '\.trae$' -and $_.Trim() -ne '') {
        $cleanPath += $_
    }
}
$env:PATH = $cleanPath -join ';'

# Remove SAFE_RM env vars
Get-ChildItem Env: -ErrorAction SilentlyContinue | Where-Object { $_.Name -like 'SAFE_RM*' } | ForEach-Object { Remove-Item "Env:$($_.Name)" -ErrorAction SilentlyContinue }

Set-Location e:\project\camera-player

$cmake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue

Write-Output "=== Configuring ==="
& $cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug 2>&1 | Out-Host
if ($LASTEXITCODE -ne 0) { Write-Output "CONFIGURE FAILED"; exit 1 }

Write-Output "=== Building ==="
& $cmake --build build --config Debug 2>&1 | Out-Host
if ($LASTEXITCODE -ne 0) { Write-Output "BUILD FAILED"; exit 1 }

Write-Output "=== SUCCESS ==="