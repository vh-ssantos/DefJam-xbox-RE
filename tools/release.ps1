# Monta o pacote para distribuir: dist\DefJam-FFNY-PC-v<versao>.zip SEM nada do jogo e SEM o jogo compilado. Quem
# baixa roda o Instalar.cmd, que monta o jogo no PC dele a partir da .iso dele (setup\instalar.ps1).
# O zip leva: Instalar.cmd, README, licencas, assets\ vazia com instrucoes e source\ (o codigo do port como esta no
# git, mais as bibliotecas que a compilacao usaria baixar com git: zydis, toml++, imgui, SDL3). Nao publica nada.
# Uso: .\release.ps1   (as bibliotecas vem de out\build\release\_deps, entao compile uma vez antes)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$deps = Join-Path $root 'out\build\release\_deps'
$version = (Select-String -Path (Join-Path $root 'CMakeLists.txt') -Pattern 'set\(DEFJAM_VERSION "([^"]+)"\)').Matches[0].Groups[1].Value
if (git -C $root status --porcelain) { throw 'ha mudancas sem commit: o pacote sai do git (HEAD)' }

$stage = Join-Path $root "dist\DefJam-FFNY-PC-v$version"
Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
$src = Join-Path $stage 'source'
New-Item -ItemType Directory -Force $src, (Join-Path $stage 'assets') | Out-Null

# o codigo como esta no commit (git archive nao leva nada que esteja no .gitignore: assets, generated, out)
$tar = Join-Path $root 'dist\source.tar'
git -C $root archive --format=tar -o $tar HEAD
& tar.exe -xf $tar -C $src
Remove-Item $tar
foreach ($d in 'zydis', 'tomlplusplus', 'imgui', 'sdl3') {
    $from = Join-Path $deps "$d-src"
    if (-not (Test-Path $from)) { throw "falta ${from} - compile uma vez antes" }
    robocopy $from (Join-Path $src "deps\$d") /E /XD .git /NFL /NDL /NJH /NJS /NP | Out-Null
}
Copy-Item (Join-Path $root 'setup\Instalar.cmd') $stage
Copy-Item (Join-Path $root 'README.md') $stage
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $stage 'LICENSE.txt')
Copy-Item (Join-Path $root 'sdk\THIRD_PARTY.txt') (Join-Path $stage 'THIRD_PARTY.txt')
$leia = 'Coloque aqui a imagem do disco (.iso) do SEU Def Jam: Fight for NY de Xbox (versao USA) e rode o Instalar.cmd.' + "`r`n" +
        'Ou rode o Instalar.cmd direto e escolha a .iso: ela e copiada para esta pasta.' + "`r`n`r`n" +
        'Put YOUR Def Jam: Fight for NY Xbox (USA) disc image (.iso) here and run Instalar.cmd, or run it and pick the .iso.'
[IO.File]::WriteAllText((Join-Path $stage 'assets\COLOQUE A ISO AQUI.txt'), $leia)
$zip = Join-Path $root "dist\DefJam-FFNY-PC-v$version.zip"
Remove-Item $zip -ErrorAction SilentlyContinue
& tar.exe -a -cf $zip -C $stage *
"SHA-256: $((Get-FileHash $zip).Hash)"
"pronto: $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 1)) MB)"
