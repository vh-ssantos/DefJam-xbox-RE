# Abre o jogo pelo live.ps1 e vai até uma luta do Battle (Blaze x Doc, Foundation), conferindo as telas no caminho.
# Uso: .\ate_luta.ps1 [-Env @{ XBE_PERF='1' }]   (variáveis de ambiente extras para esta execução)
param([hashtable]$Env = @{})
$L = Join-Path $PSScriptRoot 'live.ps1'
foreach ($k in $Env.Keys) { Set-Item "Env:$k" $Env[$k] }
& $L start | Out-Null
foreach ($k in $Env.Keys) { Remove-Item "Env:$k" }
Start-Sleep 14; & $L press 'START' | Out-Null       # EA
Start-Sleep 4;  & $L press 'START' | Out-Null       # THX / abertura
Start-Sleep 10; & $L press 'START' | Out-Null       # título
Start-Sleep 5;  & $L press 'A' | Out-Null           # aviso do autosave (com o autosave já ligado não há YES/NO)
Start-Sleep 10; & $L press 'RIGHT 0.2' | Out-Null  # Battle (0.3 s+ anda duas casas)
Start-Sleep 2;  & $L press 'A' | Out-Null
Start-Sleep 4;  & $L press 'A' | Out-Null           # One on One
Start-Sleep 4;  & $L press 'A' | Out-Null           # Standard Rules
Start-Sleep 5;  & $L press 'START' | Out-Null       # P1
Start-Sleep 3;  & $L press 'A' | Out-Null           # sem User ID
Start-Sleep 4;  & $L press 'A' | Out-Null           # continuar
Start-Sleep 5;  & $L press 'A' | Out-Null           # lutador P1
Start-Sleep 3;  & $L press 'A' | Out-Null           # roupa
Start-Sleep 3;  & $L press 'RIGHT 0.2' | Out-Null   # CPU
Start-Sleep 1;  & $L press 'A' | Out-Null
Start-Sleep 4;  & $L press 'A' | Out-Null           # arena
Start-Sleep 26
& $L shot luta
