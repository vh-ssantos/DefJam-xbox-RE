# Grava um minidump (com memória das pilhas) de um processo em execução, para ver onde ele está travado.
# Uso: .\dump_process.ps1 -ProcessId <pid> -Saida <arquivo.dmp>
param([Parameter(Mandatory)][int]$ProcessId, [Parameter(Mandatory)][string]$Saida)
Add-Type @'
using System; using System.Runtime.InteropServices; using Microsoft.Win32.SafeHandles;
public static class Dmp {
  [DllImport("dbghelp.dll", SetLastError = true)]
  public static extern bool MiniDumpWriteDump(IntPtr hProcess, uint pid, SafeFileHandle hFile, uint type,
                                              IntPtr e, IntPtr u, IntPtr c);
}
'@
$p = Get-Process -Id $ProcessId
$f = [IO.File]::Create($Saida)
# MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs
$ok = [Dmp]::MiniDumpWriteDump($p.Handle, [uint32]$ProcessId, $f.SafeFileHandle, 0x1041, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero)
$f.Close()
"dump=$ok $Saida"
