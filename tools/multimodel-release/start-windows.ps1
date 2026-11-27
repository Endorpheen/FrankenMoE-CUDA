param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('apex', 'inkling', 'bonsai')]
    [string]$Model,
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Weights,
    [ValidateRange(1, 65535)]
    [int]$Port = 9101
)

$ErrorActionPreference = 'Stop'
$File = Get-Item -LiteralPath $Weights
if ($File.PSIsContainer -or $File.Extension -ne '.gguf') {
    throw 'Weights must be a GGUF file.'
}
$Bin = 'gooseinfer-multimodel.exe'
if (-not (Test-Path (Join-Path $PSScriptRoot $Bin))) { $Bin = 'frankenmoe-multimodel.exe' }
$env:GOOSE_CUDA = '1'
$env:GOOSEINFER_WEB_ROOT = Join-Path $PSScriptRoot 'web'
$env:FRANKENMOE_WEB_ROOT = $env:GOOSEINFER_WEB_ROOT
$Context = '36864'
$Ubatch = '4096'
$Cache = @()
switch ($Model) {
    'apex' {
        $Bin = 'frankenmoe-server.exe'
        $Context = '262144'
        $Cache = @('--expert-cache-mib', '6290')
    }
    'inkling' {
        $Cache = @('--expert-cache-mib', '6720')
        Get-Content -LiteralPath (Join-Path $PSScriptRoot 'profiles/inkling.env') | ForEach-Object {
            if ($_ -match '^((?:I2_|GOOSE_|GGML_)[A-Z0-9_]+)=(.*)$') {
                [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
            }
        }
    }
    'bonsai' {
        $Ubatch = '1024'
        $env:BONSAI_CUDA_IMAGE = Join-Path $PSScriptRoot 'cuda/bonsai-http.cubin'
        $env:BONSAI_FROZEN_PTX = Join-Path $PSScriptRoot 'cuda/frozen-goose.ptx'
        $env:BONSAI_FLASH_F32 = '1'
        $env:BONSAI_FLASH_REUSE = '1'
        $env:BONSAI_FLASH_ORDERED_MERGE = '1'
        $env:BONSAI_FLASH_COMPACT = '1'
        $env:BONSAI_FLASH_ASYNC = '1'
    }
}
$LaunchOptions = @('--ctx-size', $Context, '--batch-size', '4096', '--ubatch-size', $Ubatch,
    '--cache-type-k', 'q4_0', '--cache-type-v', 'q4_0', '--threads', '8', '--threads-batch', '16')
& (Join-Path $PSScriptRoot $Bin) @LaunchOptions @Cache $File.FullName --serve "$Port"
exit $LASTEXITCODE
