# Repete o caminho título -> aviso -> YES rápido N vezes (reiniciando o jogo) e diz em quais o menu não apareceu.
# A foto de cada tentativa fica em runs\rep<i>.png. Uso: .\repro_autosave.ps1 [-Vezes 3] [-Intervalo 2.5]
param([int]$Vezes = 3, [double]$Intervalo = 2.5)
$L = Join-Path $PSScriptRoot 'live.ps1'
$runs = Join-Path (Split-Path $PSScriptRoot -Parent) 'runs'
for ($i = 1; $i -le $Vezes; $i++) {
    $env:DEFJAM_LOG = '1'
    & $L start | Out-Null
    Remove-Item Env:DEFJAM_LOG
    Start-Sleep 14; & $L press 'START' | Out-Null      # pula EA
    Start-Sleep 4;  & $L press 'START' | Out-Null      # pula THX/abertura
    Start-Sleep 10; & $L press 'START' | Out-Null      # título
    Start-Sleep 5;  & $L press 'A' | Out-Null          # aviso
    Start-Sleep $Intervalo; & $L press 'A' | Out-Null  # YES
    Start-Sleep 8
    & $L shot "rep$i" | Out-Null
    & $L stop | Out-Null
    Copy-Item (Join-Path $runs 'live.log') (Join-Path $runs "rep$i.log")
    "tentativa $i pronta"
}
