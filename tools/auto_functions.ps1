# Ciclo automático: roda o jogo, acha "add 0x... to [functions]" no registro, acrescenta em config/game.toml,
# recompila e repete. Para quando o boot não pede mais funções (ou após -Max voltas).
param([int]$Max = 25, [int]$Segundos = 25)
$root = Split-Path $PSScriptRoot -Parent
$cfg = Join-Path $root 'config\game.toml'
for ($i = 1; $i -le $Max; $i++) {
    & (Join-Path $PSScriptRoot 'boot.ps1') -Nome "auto$i" -Segundos $Segundos | Out-Null
    $log = Join-Path $root "runs\auto$i.log"
    $found = @(Select-String -Path $log -Pattern 'add 0x([0-9A-Fa-f]+) to \[functions\]' -AllMatches |
        ForEach-Object { $_.Matches } | ForEach-Object { $_.Groups[1].Value.ToUpper() } | Select-Object -Unique)
    if ($found.Count -eq 0) {
        "volta ${i}: nenhuma função faltando"
        Get-Content $log -Tail 12
        break
    }
    $text = [IO.File]::ReadAllText($cfg)
    foreach ($a in $found) {
        if ($text -notmatch "(?m)^0x$a\s*=") {
            $text = $text -replace '(?m)^\[functions\]\r?\n', "[functions]`n0x$a = `"`"`n"
        }
    }
    [IO.File]::WriteAllText($cfg, $text)
    "volta ${i}: + $($found -join ', ')"
    cmd /c (Join-Path $root 'compilar.cmd') | Out-Null
    if ($LASTEXITCODE -ne 0) { "compilação falhou"; break }
}
