param(
    [Parameter(Mandatory = $true)]
    [string]$PlatformLabel,

    [Parameter(Mandatory = $true)]
    [ValidateSet('x64', 'ARM64')]
    [string]$CMakePlatform,

    [string]$SuiteRoot = 'build_ci_windows',
    [string]$BuildDir = 'build',
    [string]$Configuration = 'Release'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-RepoPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $script:RepoRoot $Path))
}

function Invoke-LoggedNative {
    param(
        [Parameter(Mandatory = $true)]
        [string]$StepName,

        [Parameter(Mandatory = $true)]
        [string]$LogPath,

        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$ArgumentList = @()
    )

    Write-Host "[ci-windows] $StepName"
    & $FilePath @ArgumentList 2>&1 | Tee-Object -FilePath $LogPath
    $NativeExitCode = $LASTEXITCODE
    if ($NativeExitCode -ne 0) {
        $script:Failures += "$StepName=$NativeExitCode"
        $script:StepStatus[$StepName] = "failed($NativeExitCode)"
        return $false
    }

    $script:StepStatus[$StepName] = 'success'
    return $true
}

function Write-SkippedLog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LogPath,

        [Parameter(Mandatory = $true)]
        [string]$Reason
    )

    Set-Content -Path $LogPath -Value ("skipped: {0}" -f $Reason) -Encoding ascii
}

function Resolve-ExeDir {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDirAbs,

        [Parameter(Mandatory = $true)]
        [string]$ConfigurationName
    )

    $Candidates = @(
        (Join-Path $BuildDirAbs $ConfigurationName),
        $BuildDirAbs
    )

    foreach ($Candidate in $Candidates) {
        if (Test-Path (Join-Path $Candidate 'test_runtime_api.exe') -PathType Leaf) {
            return $Candidate
        }
    }

    return (Join-Path $BuildDirAbs $ConfigurationName)
}

function Write-Summary {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SummaryPath,

        [Parameter(Mandatory = $true)]
        [string]$Status,

        [Parameter(Mandatory = $true)]
        [string]$Platform,

        [Parameter(Mandatory = $true)]
        [string]$CMakePlatformText,

        [Parameter(Mandatory = $true)]
        [string]$BuildDirText,

        [Parameter(Mandatory = $true)]
        [string]$ExeDirText,

        [Parameter(Mandatory = $true)]
        [string]$LogDirText,

        [Parameter(Mandatory = $true)]
        [string]$GoldenDirText,

        [string[]]$FailedSteps = @()
    )

    $GoldenPresent = if (Get-ChildItem -Path $GoldenDirText -File -Force -ErrorAction SilentlyContinue | Select-Object -First 1) {
        'yes'
    }
    else {
        'no (exact-match run or no diff output)'
    }

    $Lines = @(
        '# CI Suite Summary',
        '',
        "- Status: $Status",
        "- Platform: $Platform",
        "- CMake platform: $CMakePlatformText",
        "- Build dir: $BuildDirText",
        "- Executable dir: $ExeDirText",
        "- Log dir: $LogDirText",
        "- Golden artifact dir: $GoldenDirText",
        "- Configure log: $LogDirText/configure.log",
        "- Build log: $LogDirText/build.log",
        "- CTest log: $LogDirText/ctest.log",
        "- Fallback log: $LogDirText/test_renderer_fallback.log",
        "- Runtime API log: $LogDirText/test_runtime_api.log",
        "- Golden runtime log: $LogDirText/test_runtime_golden.log",
        "- Golden artifacts present: $GoldenPresent"
    )

    if ($FailedSteps.Count -gt 0) {
        $Lines += "- Failed steps: $($FailedSteps -join ', ')"
    }
    else {
        $Lines += '- Failed steps: none'
    }

    Set-Content -Path $SummaryPath -Value $Lines -Encoding utf8
}

$script:RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path
$SuiteRootAbs = Resolve-RepoPath $SuiteRoot
$BuildDirAbs = Resolve-RepoPath $BuildDir
$LogDirAbs = Join-Path $SuiteRootAbs 'ci_logs'
$GoldenDirAbs = Join-Path $SuiteRootAbs 'golden_artifacts'
$SummaryPath = Join-Path $SuiteRootAbs 'ci_suite_summary.md'
$ExeDirAbs = Resolve-ExeDir -BuildDirAbs $BuildDirAbs -ConfigurationName $Configuration
$script:Failures = @()
$script:StepStatus = [ordered]@{}
$ExitCode = 0
$UnhandledError = $null

New-Item -ItemType Directory -Force -Path $SuiteRootAbs | Out-Null
New-Item -ItemType Directory -Force -Path $LogDirAbs | Out-Null
New-Item -ItemType Directory -Force -Path $GoldenDirAbs | Out-Null

Push-Location $script:RepoRoot
try {
    $ConfigureOk = Invoke-LoggedNative -StepName 'configure' -LogPath (Join-Path $LogDirAbs 'configure.log') -FilePath 'cmake' -ArgumentList @('-S', '.', '-B', $BuildDirAbs, '-A', $CMakePlatform, '-DVN_BUILD_PLAYER=ON')
    $BuildOk = $false
    $CTestOk = $false

    if ($ConfigureOk) {
        $BuildOk = Invoke-LoggedNative -StepName 'build' -LogPath (Join-Path $LogDirAbs 'build.log') -FilePath 'cmake' -ArgumentList @('--build', $BuildDirAbs, '--config', $Configuration)
    }
    else {
        Write-SkippedLog -LogPath (Join-Path $LogDirAbs 'build.log') -Reason 'configure failed'
        Write-SkippedLog -LogPath (Join-Path $LogDirAbs 'ctest.log') -Reason 'configure failed'
    }

    if ($BuildOk) {
        $CTestOk = Invoke-LoggedNative -StepName 'ctest' -LogPath (Join-Path $LogDirAbs 'ctest.log') -FilePath 'ctest' -ArgumentList @('--test-dir', $BuildDirAbs, '-C', $Configuration, '--output-on-failure')
    }
    elseif ($ConfigureOk) {
        Write-SkippedLog -LogPath (Join-Path $LogDirAbs 'ctest.log') -Reason 'build failed'
    }

    if ($BuildOk) {
        $ExeDirAbs = Resolve-ExeDir -BuildDirAbs $BuildDirAbs -ConfigurationName $Configuration
        $RerunSpecs = @(
            @{ Step = 'test_renderer_fallback'; File = 'test_renderer_fallback.exe'; Log = 'test_renderer_fallback.log' },
            @{ Step = 'test_runtime_api'; File = 'test_runtime_api.exe'; Log = 'test_runtime_api.log' },
            @{ Step = 'test_runtime_golden'; File = 'test_runtime_golden.exe'; Log = 'test_runtime_golden.log' }
        )
        $RerunOk = $true

        if (-not (Test-Path $ExeDirAbs -PathType Container)) {
            foreach ($Spec in $RerunSpecs) {
                Write-SkippedLog -LogPath (Join-Path $LogDirAbs $Spec.Log) -Reason ("executable dir missing: {0}" -f $ExeDirAbs)
            }
            $script:Failures += 'rerun-binaries=missing-exe-dir'
            $script:StepStatus['rerun-binaries'] = 'failed(missing-exe-dir)'
            $RerunOk = $false
        }
        else {
            $env:VN_GOLDEN_ARTIFACT_DIR = $GoldenDirAbs
            foreach ($Spec in $RerunSpecs) {
                $ExePath = Join-Path $ExeDirAbs $Spec.File
                $LogPath = Join-Path $LogDirAbs $Spec.Log
                if (-not (Test-Path $ExePath -PathType Leaf)) {
                    Write-SkippedLog -LogPath $LogPath -Reason ("missing executable: {0}" -f $ExePath)
                    $script:Failures += "$($Spec.Step)=missing-exe"
                    $script:StepStatus[$Spec.Step] = 'failed(missing-exe)'
                    $RerunOk = $false
                    continue
                }
                if (-not (Invoke-LoggedNative -StepName $Spec.Step -LogPath $LogPath -FilePath $ExePath)) {
                    $RerunOk = $false
                }
            }
        }

        if ($RerunOk) {
            $script:StepStatus['rerun-binaries'] = 'success'
        }
        elseif (-not $script:StepStatus.Contains('rerun-binaries')) {
            $script:StepStatus['rerun-binaries'] = 'failed(child-step)'
        }
    }
    else {
        Write-SkippedLog -LogPath (Join-Path $LogDirAbs 'test_renderer_fallback.log') -Reason 'build failed'
        Write-SkippedLog -LogPath (Join-Path $LogDirAbs 'test_runtime_api.log') -Reason 'build failed'
        Write-SkippedLog -LogPath (Join-Path $LogDirAbs 'test_runtime_golden.log') -Reason 'build failed'
        $script:StepStatus['rerun-binaries'] = 'skipped(build-failed)'
    }

    if ((-not $ConfigureOk) -or (-not $BuildOk) -or (-not $CTestOk) -or ($script:Failures.Count -gt 0)) {
        $ExitCode = 1
    }
}
catch {
    $UnhandledError = $_
    $script:Failures += ('exception=' + $_.Exception.Message)
    $ExitCode = 1
}
finally {
    Write-Summary -SummaryPath $SummaryPath -Status $(if ($ExitCode -eq 0) { 'success' } else { 'failed' }) -Platform $PlatformLabel -CMakePlatformText $CMakePlatform -BuildDirText $BuildDirAbs -ExeDirText $ExeDirAbs -LogDirText $LogDirAbs -GoldenDirText $GoldenDirAbs -FailedSteps $script:Failures
    Pop-Location
}

if ($UnhandledError -ne $null) {
    Write-Host "[ci-windows] error: $($UnhandledError.Exception.Message)"
}

exit $ExitCode
