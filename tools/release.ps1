# Monta o pacote para distribuir (padrao do port do BLACK): dist\DefJam-FFNY-PC-v<versao>.zip com o exe SEM o icone
# do jogo embutido (o exe tira o icone da copia do jogador ao abrir), uma pasta assets vazia com instrucoes, o README
# e as licencas. Recompila so o necessario (troca o DEFJAM_EMBED_ICON e volta). Nao publica nada.
# Uso: .\release.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'out\build\release'
$version = (Select-String -Path (Join-Path $root 'CMakeLists.txt') -Pattern 'set\(DEFJAM_VERSION "([^"]+)"\)').Matches[0].Groups[1].Value

# compilar.cmd com a opcao do icone; o aviso do vswhere no stderr nao e erro, entao o cmd escreve tudo no log
function Build([string]$embed) {
    $env:DEFJAM_EMBED_ICON = $embed
    $ErrorActionPreference = 'Continue'
    cmd /c "`"$root\compilar.cmd`" > nul 2>&1"
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    Remove-Item Env:DEFJAM_EMBED_ICON
    if ($code -ne 0) { throw 'falhou a compilacao (out\compilar.log)' }
}

"versao ${version}: compilando sem o icone do jogo"
Build 'OFF'
$stage = Join-Path $root "dist\DefJam-FFNY-PC-v$version"
Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force (Join-Path $stage 'assets') | Out-Null
Copy-Item (Join-Path $build 'DefJamFFNY.exe') $stage
Copy-Item (Join-Path $root 'README.md') $stage
Copy-Item (Join-Path $root 'sdk\LICENSE') (Join-Path $stage 'LICENSE-XboxXBE-Recomp.txt')
Copy-Item (Join-Path $root 'sdk\THIRD_PARTY.txt') (Join-Path $stage 'THIRD_PARTY.txt')
$leia = 'Coloque aqui a imagem do disco (.iso) do SEU Def Jam: Fight for NY de Xbox (versao USA).' + "`r`n" +
        'Ou abra o DefJamFFNY.exe sem nada aqui: ele pergunta e voce escolhe a .iso, que e copiada para esta pasta.'
[IO.File]::WriteAllText((Join-Path $stage 'assets\COLOQUE A ISO AQUI.txt'), $leia)
$zip = Join-Path $root "dist\DefJam-FFNY-PC-v$version.zip"
Remove-Item $zip -ErrorAction SilentlyContinue
# o exe na raiz do zip (a atualizacao automatica extrai DefJamFFNY.exe de la)
& tar.exe -a -cf $zip -C $stage *
"pronto: $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 1)) MB)"
"voltando ao exe com o icone embutido (uso local)"
Build 'ON'
