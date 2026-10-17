# Boot de teste do port (Xbox) na tela 2, sem teclas: espera N segundos, captura só a janela do jogo e fecha.
# Uso: .\boot.ps1 -Nome xboot1 [-Segundos 60] [-Capturas 20,40,58]
param([Parameter(Mandatory)]$Nome, [int]$Segundos = 60, [int[]]$Capturas = @())
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exeDir = Join-Path $root 'out\build\release'
$runs = Join-Path $root 'runs'
New-Item -ItemType Directory -Force $runs | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class W {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
'@
$env:DEFJAM_WINDOW_MONITOR = '1'
$p = Start-Process -FilePath (Join-Path $exeDir 'DefJamFFNY.exe') -WorkingDirectory $exeDir -PassThru
$t0 = Get-Date
foreach ($c in ($Capturas | Sort-Object)) {
    while (((Get-Date) - $t0).TotalSeconds -lt $c) { Start-Sleep -Milliseconds 250 }
    $p.Refresh()
    if ($p.HasExited) { break }
    $h = $p.MainWindowHandle
    if ($h -eq [IntPtr]::Zero) { continue }
    $r = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $hh = $r.B - $r.T
    if ($w -le 0 -or $hh -le 0) { continue }
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc(); [W]::PrintWindow($h, $hdc, 2) | Out-Null; $g.ReleaseHdc($hdc); $g.Dispose()
    $bmp.Save((Join-Path $runs "${Nome}_$c.png"), [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
}
while (((Get-Date) - $t0).TotalSeconds -lt $Segundos -and -not $p.HasExited) { Start-Sleep -Milliseconds 500; $p.Refresh() }
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
Remove-Item Env:DEFJAM_WINDOW_MONITOR
$log = Get-ChildItem (Join-Path $exeDir 'logs') -Filter '*.log' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
if ($log) { Copy-Item $log.FullName (Join-Path $runs "$Nome.log") }
"saiu=$($p.HasExited) codigo=$($p.ExitCode) log=$($log.Name)"
