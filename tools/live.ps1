# Teste ao vivo do port (tela 2, sem teclas no PC): os botões vão por arquivo, lido pelo próprio jogo (DEFJAM_PAD_CMD).
#   .\live.ps1 start            abre o jogo e volta na hora
#   .\live.ps1 press "A 0.2" "DOWN" ...   enfileira botões (linha = "botões [segundos]"; WAIT n; CAPTURE)
#   .\live.ps1 shot <nome>      foto só da janela em runs\<nome>.png
#   .\live.ps1 stop             fecha o jogo e copia o log para runs\live.log
param([Parameter(Mandatory)][ValidateSet('start', 'press', 'shot', 'stop')]$Acao, [Parameter(ValueFromRemainingArguments)]$Resto)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = Join-Path $root 'out\build\release'
$runs = Join-Path $root 'runs'
$cmd = Join-Path $runs 'pad_cmd.txt'
New-Item -ItemType Directory -Force $runs | Out-Null
function Game { Get-Process DefJamFFNY -ErrorAction SilentlyContinue | Select-Object -First 1 }
switch ($Acao) {
    'start' {
        if (Game) { 'já está rodando'; return }
        Remove-Item $cmd -ErrorAction SilentlyContinue
        $env:DEFJAM_WINDOW_MONITOR = '1'
        $env:DEFJAM_PAD_CMD = $cmd
        $p = Start-Process -FilePath (Join-Path $exeDir 'DefJamFFNY.exe') -WorkingDirectory $exeDir -PassThru
        Remove-Item Env:DEFJAM_WINDOW_MONITOR, Env:DEFJAM_PAD_CMD
        "pid=$($p.Id)"
    }
    'press' {
        # acrescenta (o jogo apaga o arquivo depois de ler)
        Add-Content -Path $cmd -Value ($Resto -join "`n") -Encoding ascii
        "ok"
    }
    'shot' {
        $p = Game
        if (-not $p) { 'jogo não está rodando'; return }
        Add-Type -AssemblyName System.Drawing
        if (-not ('W2' -as [type])) {
            Add-Type @'
using System; using System.Runtime.InteropServices;
public static class W2 {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
'@
        }
        $h = $p.MainWindowHandle
        $r = New-Object W2+RECT; [W2]::GetWindowRect($h, [ref]$r) | Out-Null
        $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $hdc = $g.GetHdc(); [W2]::PrintWindow($h, $hdc, 2) | Out-Null; $g.ReleaseHdc($hdc); $g.Dispose()
        $out = Join-Path $runs "$($Resto[0]).png"
        $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
        $out
    }
    'stop' {
        $p = Game
        if ($p) { Stop-Process -Id $p.Id -Force; Start-Sleep 1 }
        $log = Get-ChildItem (Join-Path $exeDir 'logs') -Filter '*.log' | Sort-Object LastWriteTime | Select-Object -Last 1
        if ($log) { Copy-Item $log.FullName (Join-Path $runs 'live.log') }
        "parado"
    }
}
