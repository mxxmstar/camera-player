param(
    [Parameter(Position=0)]
    [ValidateSet('build', 'rebuild', 'clean', 'configure', 'help')]
    [string]$Action = 'build',

    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug'
)

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
$buildDir = "build"

# Release 产物输出到源码根目录 bin/；Debug 产物输出到 build/Debug/
function Get-OutputDir {
    if ($Config -eq 'Release') { return "bin" } else { return "$buildDir\Debug" }
}

function Invoke-CMakeConfigure {
    Write-Output "=== Configuring ($Config) ==="
    & $cmake -S . -B $buildDir 2>&1
    if ($LASTEXITCODE -ne 0) { Write-Output "CONFIGURE FAILED"; exit 1 }
}

function Invoke-CMakeBuild {
    Write-Output "=== Building ($Config) ==="
    & $cmake --build $buildDir --config $Config 2>&1
    if ($LASTEXITCODE -ne 0) { Write-Output "BUILD FAILED"; exit 1 }
}

function Remove-BuildDir {
    if (Test-Path $buildDir) {
        Write-Output "=== Removing $buildDir ==="
        Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue
    }
}

# Release 产物位于源码根目录 bin/，cmake clean 不会清理 post-build 复制的 DLL，
# 因此 Release 清理时直接删除整个 bin 目录。
function Remove-BinDir {
    if (Test-Path "bin") {
        Write-Output "=== Removing bin ==="
        Remove-Item -Recurse -Force "bin" -ErrorAction SilentlyContinue
    }
}

function Show-Success {
    param([string]$Label)
    $outDir = Get-OutputDir
    Write-Output "=== $Label SUCCESS ==="
    Write-Output "Binary: $outDir\camera-player.exe"
    Write-Output "Run:    $outDir\camera-player.exe"
}

switch ($Action) {
    'configure' {
        Invoke-CMakeConfigure
        Write-Output "=== CONFIGURE SUCCESS ==="
    }
    'build' {
        # 增量编译：未配置时先 configure，再 build
        if (-not (Test-Path "$buildDir\CMakeCache.txt")) {
            Invoke-CMakeConfigure
        }
        Invoke-CMakeBuild
        Show-Success "BUILD"
    }
    'rebuild' {
        # 重新编译：清除 build 目录后重新 configure + build
        Remove-BuildDir
        if ($Config -eq 'Release') { Remove-BinDir }
        Invoke-CMakeConfigure
        Invoke-CMakeBuild
        Show-Success "REBUILD"
    }
    'clean' {
        # 清除：Debug 调用 cmake clean target；Release 删除 bin 目录
        if ($Config -eq 'Release') {
            Remove-BinDir
            Write-Output "=== CLEAN SUCCESS (Release: bin removed) ==="
        } elseif (Test-Path "$buildDir\CMakeCache.txt") {
            Write-Output "=== Cleaning ($Config) ==="
            & $cmake --build $buildDir --config $Config --target clean 2>&1
            if ($LASTEXITCODE -ne 0) { Write-Output "CLEAN FAILED"; exit 1 }
            Write-Output "=== CLEAN SUCCESS ==="
        } else {
            Write-Output "Build directory not configured. Nothing to clean."
        }
    }
    'help' {
        Write-Output "Usage: .\build.ps1 [action] [-Config <Debug|Release>]"
        Write-Output ""
        Write-Output "Actions:"
        Write-Output "  build      增量编译（默认）"
        Write-Output "  rebuild    重新编译（清除 build 目录后重新构建）"
        Write-Output "  clean      清除构建产物"
        Write-Output "  configure  仅运行 CMake 配置"
        Write-Output "  help       显示帮助"
        Write-Output ""
        Write-Output "Options:"
        Write-Output "  -Config    Debug（默认）或 Release"
        Write-Output ""
        Write-Output "Examples:"
        Write-Output "  .\build.ps1                          # 增量编译 Debug"
        Write-Output "  .\build.ps1 rebuild                  # 重新编译 Debug"
        Write-Output "  .\build.ps1 build -Config Release    # 增量编译 Release（产物输出到 bin/）"
        Write-Output "  .\build.ps1 clean -Config Release    # 清除 Release 产物（删除 bin/）"
        Write-Output "  .\build.ps1 rebuild -Config Release  # 重新编译 Release"
    }
}
