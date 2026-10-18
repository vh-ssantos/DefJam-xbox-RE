# Repete "abrir o jogo -> luta" N vezes e diz, em cada vez, se o jogo travou (fechou) ou congelou (imagem parada).
# Uso: .\estresse_luta.ps1 [-Vezes 4]
param([int]$Vezes = 4)
$L = Join-Path $PSScriptRoot 'live.ps1'
$runs = Join-Path (Split-Path $PSScriptRoot -Parent) 'runs'
$exeDir = Join-Path (Split-Path $PSScriptRoot -Parent) 'out\build\release'
for ($i = 1; $i -le $Vezes; $i++) {
    & $L stop | Out-Null
    Start-Sleep 2
    $antes = @(Get-ChildItem (Join-Path $exeDir 'CrashDumps') -ErrorAction SilentlyContinue).Count
    & (Join-Path $PSScriptRoot 'ate_luta.ps1') | Out-Null
    $vivo = [bool](Get-Process DefJamFFNY -ErrorAction SilentlyContinue)
    $parado = $false
    if ($vivo) {
        & $L press 'X 0.2' 'Y 0.2' | Out-Null
        & $L shot "est_a" | Out-Null; Start-Sleep 3; & $L shot "est_b" | Out-Null
        $parado = (Get-FileHash (Join-Path $runs 'est_a.png')).Hash -eq (Get-FileHash (Join-Path $runs 'est_b.png')).Hash
    }
    $depois = @(Get-ChildItem (Join-Path $exeDir 'CrashDumps') -ErrorAction SilentlyContinue).Count
    "rodada ${i}: vivo=$vivo congelado=$parado crashdumps_novos=$($depois - $antes)"
    Copy-Item (Join-Path $runs 'luta.png') (Join-Path $runs "est_$i.png") -ErrorAction SilentlyContinue
}
& $L stop | Out-Null
