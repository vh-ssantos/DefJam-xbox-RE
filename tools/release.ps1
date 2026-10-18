# Monta o pacote para distribuir (padrão do port do BLACK): dist\DefJam-FFNY-PC-v<versão>.zip com o exe SEM o ícone
# do jogo embutido (o exe tira o ícone da cópia do jogador ao abrir), uma pasta assets vazia com instruções, o README
# e as licenças. Recompila só o necessário (troca o DEFJAM_EMBED_ICON e volta). Não publica nada.
# Uso: .\release.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'out\build\release'
$version = (Select-String -Path (Join-Path $root 'CMakeLists.txt') -Pattern 'set\(DEFJAM_VERSION "([^"]+)"\)').Matches[0].Groups[1].Value
$env_cmd = 'call "E:\Program Files\Visual Studio\VC\Auxiliary\Build\vcvars64.bat" >nul && set "PATH=E:\Tools\llvm\bin;E:\Program Files\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;E:\Program Files\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"'
function Build([string]$embed) {
    cmd /c "$env_cmd && cd /d `"$root`" && cmake --preset release -DDEFJAM_EMBED_ICON=$embed >nul && cmake --build out\build\release >> out\release.log 2>&1"
    if ($LASTEXITCODE -ne 0) { throw "falhou a compilação (out\release.log)" }
}
"versão $version: compilando sem o ícone do jogo"
Build 'OFF'
$stage = Join-Path $root "dist\DefJam-FFNY-PC-v$version"
Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force (Join-Path $stage 'assets') | Out-Null
Copy-Item (Join-Path $build 'DefJamFFNY.exe') $stage
Copy-Item (Join-Path $root 'README.md') $stage
Copy-Item (Join-Path $root 'sdk\LICENSE') (Join-Path $stage 'LICENSE-XboxXBE-Recomp.txt')
Copy-Item (Join-Path $root 'sdk\THIRD_PARTY.txt') (Join-Path $stage 'THIRD_PARTY.txt')
Set-Content -Encoding utf8 (Join-Path $stage 'assets\COLOQUE A ISO AQUI.txt') @"
Coloque aqui a imagem do disco (.iso) do SEU Def Jam: Fight for NY de Xbox (versão USA).
Ou abra o DefJamFFNY.exe sem nada aqui: ele pergunta e você escolhe a .iso, que é copiada para esta pasta.
"@
$zip = Join-Path $root "dist\DefJam-FFNY-PC-v$version.zip"
Remove-Item $zip -ErrorAction SilentlyContinue
# o exe na raiz do zip (a atualização automática extrai DefJamFFNY.exe de lá)
& tar.exe -a -cf $zip -C $stage *
"pronto: $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 1)) MB)"
"voltando ao exe com o ícone embutido (uso local)"
Build 'ON'
