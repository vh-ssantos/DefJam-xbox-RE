# Instalador do Def Jam: Fight for NY (port de PC). Monta o jogo NO SEU PC a partir da sua própria .iso do Xbox:
# nenhum código do jogo vem pela internet. Passos: escolher a .iso, conferir que é a versão USA, conferir/instalar as
# ferramentas gratuitas de compilação da Microsoft (Visual Studio Build Tools), compilar e criar o atalho.
#
# Pastas (a do zip descompactado é a raiz):
#   <raiz>\DefJamFFNY.exe      o jogo, depois de compilado
#   <raiz>\assets\*.iso        a sua .iso (o jogo lê dela)
#   <raiz>\hd\                texturas HD opcionais, geradas somente da sua .iso
#   <raiz>\source\             o código do port, as bibliotecas (deps) e o que a compilação gera
#
# Uso: Instalar.cmd (com janela). Instalar.cmd -Update: recompila depois de uma atualização, sem perguntar nada.
# Instalar.cmd -Iso <arquivo> [-Auto] [-HdTextures]: ISO escolhida; -Auto começa sozinho; HD é opcional.
# DEFJAM_SETUP_LLVM=<pasta bin do LLVM>: usa esse clang em vez do do Visual Studio (testes do autor).
param([switch]$Update, [string]$Iso, [switch]$Auto, [switch]$HdTextures)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms, System.Drawing

$Source = Split-Path $PSScriptRoot -Parent
$Root = Split-Path $Source -Parent
$Assets = Join-Path $Root 'assets'
$ExeName = 'DefJamFFNY.exe'
$XbeSha1 = '4352dc8438bce03e792fab51e5cbf24ef492f770'  # default.xbe da versão USA
$Pt = (Get-UICulture).TwoLetterISOLanguageName -eq 'pt'
function T([string]$pt, [string]$en) { if ($Pt) { $pt } else { $en } }

$LogDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force $LogDir | Out-Null
$LogFile = Join-Path $LogDir ("instalacao-{0:yyyyMMdd-HHmmss}.log" -f (Get-Date))

# --- ajudantes em C#: ler a .iso e rodar processos sem travar a janela
Add-Type -TypeDefinition @'
using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading.Tasks;

public static class DjIso {
    const int Sector = 2048;
    static readonly long[] Partitions = { 0, 0x18300000 };
    // o default.xbe na raiz da imagem (formato XDVDFS: tabela de entradas em árvore binária)
    public static byte[] ReadXbe(string iso) {
        using (var f = new FileStream(iso, FileMode.Open, FileAccess.Read, FileShare.Read)) {
            foreach (long b in Partitions) {
                var vol = new byte[Sector];
                f.Position = b + 32 * Sector;
                if (f.Read(vol, 0, Sector) != Sector) continue;
                if (Encoding.ASCII.GetString(vol, 0, 20) != "MICROSOFT*XBOX*MEDIA") continue;
                uint sector = BitConverter.ToUInt32(vol, 0x14), size = BitConverter.ToUInt32(vol, 0x18);
                var table = new byte[size];
                f.Position = b + (long)sector * Sector;
                f.Read(table, 0, (int)size);
                var todo = new System.Collections.Generic.Stack<int>();
                todo.Push(0);
                int guard = 0;
                while (todo.Count > 0 && guard++ < 100000) {
                    int at = todo.Pop();
                    if (at + 14 > table.Length) continue;
                    ushort left = BitConverter.ToUInt16(table, at), right = BitConverter.ToUInt16(table, at + 2);
                    if (left == 0xFFFF) continue;
                    uint fs = BitConverter.ToUInt32(table, at + 4), fl = BitConverter.ToUInt32(table, at + 8);
                    int len = table[at + 13];
                    string name = Encoding.ASCII.GetString(table, at + 14, Math.Min(len, table.Length - at - 14));
                    if (string.Equals(name, "default.xbe", StringComparison.OrdinalIgnoreCase)) {
                        var x = new byte[fl];
                        f.Position = b + (long)fs * Sector;
                        f.Read(x, 0, (int)fl);
                        return x;
                    }
                    if (left != 0) todo.Push(left * 4);
                    if (right != 0) todo.Push(right * 4);
                }
            }
        }
        return null;
    }
}

public class DjCopy {
    public volatile bool Done;
    public long Copied, Total;
    public string Error;
    public void Start(string from, string to) {
        Task.Run(() => {
            try {
                using (var i = new FileStream(from, FileMode.Open, FileAccess.Read, FileShare.Read))
                using (var o = new FileStream(to + ".parcial", FileMode.Create, FileAccess.Write)) {
                    Total = i.Length;
                    var buf = new byte[4 << 20];
                    int n;
                    while ((n = i.Read(buf, 0, buf.Length)) > 0) { o.Write(buf, 0, n); Copied += n; }
                }
                if (File.Exists(to)) File.Delete(to);
                File.Move(to + ".parcial", to);
            } catch (Exception e) { Error = e.Message; }
            Done = true;
        });
    }
}

public class DjProc {
    public ConcurrentQueue<string> Lines = new ConcurrentQueue<string>();
    public Process P;
    public void Start(string file, string args, string dir) {
        var si = new ProcessStartInfo(file, args) {
            WorkingDirectory = dir, UseShellExecute = false, CreateNoWindow = true,
            RedirectStandardOutput = true, RedirectStandardError = true,
            StandardOutputEncoding = Encoding.Default, StandardErrorEncoding = Encoding.Default
        };
        P = new Process { StartInfo = si };
        P.OutputDataReceived += (s, e) => { if (e.Data != null) Lines.Enqueue(e.Data); };
        P.ErrorDataReceived += (s, e) => { if (e.Data != null) Lines.Enqueue(e.Data); };
        P.Start();
        P.BeginOutputReadLine();
        P.BeginErrorReadLine();
    }
}
'@

# --- janela
$form = New-Object Windows.Forms.Form
$form.Text = T 'Def Jam: Fight for NY - Instalador' 'Def Jam: Fight for NY - Setup'
$form.Size = New-Object Drawing.Size(720, 592)
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$font = New-Object Drawing.Font('Segoe UI', 10)
$form.Font = $font

$intro = New-Object Windows.Forms.Label
$intro.Location = '16,12'; $intro.Size = '672,92'
$intro.Text = T ("O jogo é montado aqui no seu PC a partir da SUA .iso do Def Jam: Fight for NY de Xbox (versão USA). " +
    "Nada do jogo vem pela internet.`n`nA primeira instalação baixa as ferramentas gratuitas da Microsoft (alguns GB, " +
    "uma vez só) e compila por alguns minutos (uns 3 num PC rápido, mais num PC simples). Pode usar o PC enquanto isso.") `
    ("The game is built here on your PC from YOUR Def Jam: Fight for NY Xbox .iso (USA version). Nothing from the " +
    "game comes from the internet.`n`nThe first install downloads Microsoft's free build tools (a few GB, once) and " +
    "compiles for a few minutes (about 3 on a fast PC, longer on a modest one). You can keep using your PC meanwhile.")
$form.Controls.Add($intro)

$isoLabel = New-Object Windows.Forms.Label
$isoLabel.Location = '16,112'; $isoLabel.Size = '672,22'
$isoLabel.Text = T 'Sua .iso do jogo (ou o default.xbe de uma cópia extraída):' 'Your game .iso (or default.xbe of an extracted copy):'
$form.Controls.Add($isoLabel)
$isoBox = New-Object Windows.Forms.TextBox
$isoBox.Location = '16,136'; $isoBox.Size = '560,26'
$form.Controls.Add($isoBox)
$isoBtn = New-Object Windows.Forms.Button
$isoBtn.Location = '584,134'; $isoBtn.Size = '104,30'
$isoBtn.Text = T 'Escolher...' 'Browse...'
$form.Controls.Add($isoBtn)

$shortcut = New-Object Windows.Forms.CheckBox
$shortcut.Location = '16,172'; $shortcut.Size = '672,24'; $shortcut.Checked = $true
$shortcut.Text = T 'Criar atalho na área de trabalho' 'Create a desktop shortcut'
$form.Controls.Add($shortcut)

$hdCheck = New-Object Windows.Forms.CheckBox
$hdCheck.Location = '16,202'; $hdCheck.Size = '672,24'; $hdCheck.Checked = [bool]$HdTextures
$hdCheck.Text = T 'Gerar texturas HD (mais ~6 GB e alguns minutos)' 'Generate HD textures (about 6 GB more and a few minutes)'
$form.Controls.Add($hdCheck)

$status = New-Object Windows.Forms.Label
$status.Location = '16,236'; $status.Size = '672,22'
$form.Controls.Add($status)
$bar = New-Object Windows.Forms.ProgressBar
$bar.Location = '16,262'; $bar.Size = '672,22'; $bar.Maximum = 1000
$form.Controls.Add($bar)
$logBox = New-Object Windows.Forms.TextBox
$logBox.Location = '16,292'; $logBox.Size = '672,200'
$logBox.Multiline = $true; $logBox.ReadOnly = $true; $logBox.ScrollBars = 'Vertical'
$logBox.Font = New-Object Drawing.Font('Consolas', 8.5)
$form.Controls.Add($logBox)

$go = New-Object Windows.Forms.Button
$go.Location = '16,502'; $go.Size = '160,36'
$go.Text = T 'Instalar' 'Install'
$form.Controls.Add($go)
$play = New-Object Windows.Forms.Button
$play.Location = '184,502'; $play.Size = '160,36'; $play.Enabled = $false
$play.Text = T 'Jogar' 'Play'
$form.Controls.Add($play)
$logs = New-Object Windows.Forms.Button
$logs.Location = '528,502'; $logs.Size = '160,36'
$logs.Text = T 'Abrir registro' 'Open log'
$form.Controls.Add($logs)

function Log([string]$s) {
    Add-Content -Path $LogFile -Value $s -Encoding UTF8
    $logBox.AppendText($s + "`r`n")
}
function Status([string]$s) { $status.Text = $s; Log "== $s"; [Windows.Forms.Application]::DoEvents() }
function Pump { [Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 40 }
function Fail([string]$s) {
    Log "ERRO: $s"
    $status.Text = T 'Não deu certo. Veja o registro.' 'It did not work. See the log.'
    [Windows.Forms.MessageBox]::Show($form, $s, $form.Text, 'OK', 'Error') | Out-Null
    throw "falhou"
}

# a .iso já em assets
$found = Get-ChildItem $Assets -Filter *.iso -ErrorAction SilentlyContinue | Select-Object -First 1
if ($found) { $isoBox.Text = $found.FullName }
elseif (Test-Path (Join-Path $Assets 'default.xbe')) { $isoBox.Text = Join-Path $Assets 'default.xbe' }

$isoBtn.Add_Click({
    $d = New-Object Windows.Forms.OpenFileDialog
    $d.Filter = T 'Imagem do disco (*.iso)|*.iso|default.xbe|default.xbe|Todos os arquivos|*.*' 'Disc image (*.iso)|*.iso|default.xbe|default.xbe|All files|*.*'
    $d.Title = T 'Escolha a .iso do Def Jam: Fight for NY (Xbox, USA)' 'Pick the Def Jam: Fight for NY (Xbox, USA) .iso'
    if ($d.ShowDialog($form) -eq 'OK') { $isoBox.Text = $d.FileName }
})
$logs.Add_Click({ Start-Process notepad.exe $LogFile })
$play.Add_Click({ Start-Process -FilePath (Join-Path $Root $ExeName) -WorkingDirectory $Root; $form.Close() })

# --- passos
function Get-Sha1([byte[]]$b) {
    $h = [Security.Cryptography.SHA1]::Create().ComputeHash($b)
    -join ($h | ForEach-Object { $_.ToString('x2') })
}

function Step-Game {
    $path = $isoBox.Text.Trim('"', ' ')
    if (-not $path -or -not (Test-Path $path)) { Fail (T 'Escolha a .iso do jogo primeiro.' 'Pick the game .iso first.') }
    if ($hdCheck.Checked -and [IO.Path]::GetExtension($path) -ine '.iso') {
        Fail (T 'Para gerar texturas HD, escolha a .iso original do jogo.' 'To generate HD textures, pick the original game .iso.')
    }
    $script:HdIso = $path
    New-Item -ItemType Directory -Force $Assets, (Join-Path $Source 'assets') | Out-Null
    if ($path -like '*.xbe') {
        $xbe = [IO.File]::ReadAllBytes($path)
        $gameDir = Split-Path $path -Parent
    } else {
        Status (T 'Lendo a .iso...' 'Reading the .iso...')
        $xbe = [DjIso]::ReadXbe($path)
        if (-not $xbe) { Fail (T 'Esse arquivo não é uma imagem de disco do Xbox.' 'This file is not an Xbox disc image.') }
    }
    $sha = Get-Sha1 $xbe
    Log "default.xbe sha1 $sha"
    if ($sha -ne $XbeSha1) {
        $r = [Windows.Forms.MessageBox]::Show($form, (T "Essa não é a versão USA de Xbox do Def Jam: Fight for NY que este port suporta. O jogo provavelmente não vai funcionar.`n`nContinuar mesmo assim?" "This is not the USA Xbox release of Def Jam: Fight for NY that this port supports. The game will most likely not work.`n`nContinue anyway?"), $form.Text, 'YesNo', 'Warning')
        if ($r -ne 'Yes') { Fail (T 'Instalação cancelada.' 'Install cancelled.') }
    }
    # o compilador lê o default.xbe de source\assets
    [IO.File]::WriteAllBytes((Join-Path $Source 'assets\default.xbe'), $xbe)
    if ($path -like '*.xbe') {
        # cópia extraída: as pastas do jogo vão para assets
        if ((Resolve-Path $gameDir).Path -ne (Resolve-Path $Assets).Path) {
            Status (T 'Copiando os arquivos do jogo...' 'Copying the game files...')
            Copy-Item -Path (Join-Path $gameDir '*') -Destination $Assets -Recurse -Force
        }
        return
    }
    $dest = Join-Path $Assets (Split-Path $path -Leaf)
    $script:HdIso = $dest
    if ((Test-Path $dest) -and (Get-Item $dest).Length -eq (Get-Item $path).Length) { return }
    Status (T 'Copiando a .iso para a pasta assets...' 'Copying the .iso to the assets folder...')
    $c = New-Object DjCopy
    $c.Start($path, $dest)
    while (-not $c.Done) {
        if ($c.Total) { $bar.Value = [int](1000 * $c.Copied / $c.Total) }
        Pump
    }
    if ($c.Error) { Fail ((T 'Não consegui copiar a .iso: ' 'Could not copy the .iso: ') + $c.Error) }
}

$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$Components = @('Microsoft.VisualStudio.Component.VC.Tools.x86.x64', 'Microsoft.VisualStudio.Component.VC.Llvm.Clang',
    'Microsoft.VisualStudio.Component.VC.CMake.Project')

# um Visual Studio (ou Build Tools) com tudo o que a compilação usa; -All: qualquer um com C++ (para completar)
function Find-Vs([switch]$All) {
    if (-not (Test-Path $VsWhere)) { return $null }
    $req = if ($All -or $env:DEFJAM_SETUP_LLVM) { @($Components[0]) } else { $Components }
    & $VsWhere -latest -products * -requires $req -property installationPath | Select-Object -First 1
}

function Step-Tools {
    Status (T 'Conferindo as ferramentas de compilação da Microsoft...' 'Checking Microsoft build tools...')
    $vs = Find-Vs
    if ($vs) { Log "Visual Studio: $vs"; return $vs }
    $msg = T ("Faltam as ferramentas gratuitas de compilação da Microsoft (Visual Studio Build Tools, com C++, Clang e CMake).`n`n" +
        "Posso baixar e instalar agora pelo instalador oficial da Microsoft? O Windows vai pedir permissão de administrador. " +
        "São alguns GB.") `
        ("Microsoft's free build tools are missing (Visual Studio Build Tools with C++, Clang and CMake).`n`n" +
        "Download and install them now with Microsoft's official installer? Windows will ask for administrator " +
        "permission. It is a few GB.")
    if ([Windows.Forms.MessageBox]::Show($form, $msg, $form.Text, 'YesNo', 'Question') -ne 'Yes') {
        Fail (T 'Sem as ferramentas não dá para montar o jogo.' 'The game cannot be built without the tools.')
    }
    $adds = @()
    foreach ($c in $Components) { $adds += '--add'; $adds += $c }
    $existing = Find-Vs -All
    if ($existing) {
        Status (T 'Acrescentando o que falta ao seu Visual Studio (aceite a permissão do Windows)...' 'Adding the missing parts to your Visual Studio (accept the Windows prompt)...')
        $setup = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\setup.exe'
        $argl = @('modify', '--installPath', "`"$existing`"") + $adds + @('--passive', '--norestart')
        $p = Start-Process -FilePath $setup -ArgumentList $argl -Verb RunAs -PassThru
    } else {
        Status (T 'Baixando o instalador da Microsoft...' 'Downloading the Microsoft installer...')
        $bt = Join-Path $env:TEMP 'vs_BuildTools.exe'
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        (New-Object Net.WebClient).DownloadFile('https://aka.ms/vs/17/release/vs_BuildTools.exe', $bt)
        $sig = Get-AuthenticodeSignature $bt
        if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notlike '*Microsoft Corporation*') {
            Fail (T 'O instalador baixado não tem a assinatura da Microsoft. Parei por segurança.' 'The downloaded installer is not signed by Microsoft. Stopped for safety.')
        }
        Status (T 'Instalando as ferramentas da Microsoft (aceite a permissão do Windows; demora)...' 'Installing Microsoft tools (accept the Windows prompt; this takes a while)...')
        $argl = @('--passive', '--wait', '--norestart', '--add', 'Microsoft.VisualStudio.Workload.VCTools', '--includeRecommended') + $adds
        $p = Start-Process -FilePath $bt -ArgumentList $argl -Verb RunAs -PassThru
    }
    $bar.Style = 'Marquee'
    while (-not $p.HasExited) { Pump }
    $bar.Style = 'Continuous'
    Log "instalador da Microsoft saiu com $($p.ExitCode)"
    $vs = Find-Vs
    if (-not $vs) { Fail (T 'As ferramentas não ficaram instaladas. Tente de novo, ou instale o Visual Studio Build Tools com C++, Clang e CMake.' 'The tools did not get installed. Try again, or install Visual Studio Build Tools with C++, Clang and CMake.') }
    Log "Visual Studio: $vs"
    return $vs
}

function Step-Build([string]$vs) {
    $cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
    $llvm = if ($env:DEFJAM_SETUP_LLVM) { $env:DEFJAM_SETUP_LLVM } else { Join-Path $vs 'VC\Tools\Llvm\x64\bin' }
    $deps = Join-Path $Source 'deps'
    $cfg = @('--preset release', '-DDEFJAM_EMBED_ICON=OFF', '-DFETCHCONTENT_FULLY_DISCONNECTED=ON',
        "-DFETCHCONTENT_SOURCE_DIR_ZYDIS=`"$deps\zydis`"", "-DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=`"$deps\tomlplusplus`"",
        "-DFETCHCONTENT_SOURCE_DIR_IMGUI=`"$deps\imgui`"", "-DFETCHCONTENT_SOURCE_DIR_SDL3=`"$deps\sdl3`"") -join ' '
    $cmd = Join-Path $Source 'out\setup-build.cmd'
    New-Item -ItemType Directory -Force (Split-Path $cmd) | Out-Null
    @"
@echo off
call "$vs\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64 >nul || exit /b 1
set "PATH=$llvm;$cmake\CMake\bin;$cmake\Ninja;%PATH%"
cd /d "$Source"
echo configurando
cmake $cfg || exit /b 1
echo compilando
cmake --build out\build\release || exit /b 1
"@ | Set-Content -Path $cmd -Encoding Default
    Status (T 'Compilando o jogo (alguns minutos na primeira vez)...' 'Building the game (a few minutes the first time)...')
    $r = New-Object DjProc
    $r.Start($env:ComSpec, "/c `"$cmd`"", $Source)
    $line = $null
    $start = Get-Date
    while (-not $r.P.HasExited -or $r.Lines.Count) {
        while ($r.Lines.TryDequeue([ref]$line)) {
            Add-Content -Path $LogFile -Value $line -Encoding UTF8
            if ($line -match '^\[(\d+)/(\d+)\]') {
                $bar.Value = [Math]::Min(1000, [int](1000 * [int]$Matches[1] / [int]$Matches[2]))
                $status.Text = (T 'Compilando' 'Building') + " $($Matches[1])/$($Matches[2]) - " + ('{0:hh\:mm\:ss}' -f ((Get-Date) - $start))
            } elseif ($line -match 'error|FAILED|configurando|compilando|warning: unused' -and $line -notmatch '^\s*$') {
                $logBox.AppendText($line + "`r`n")
            }
        }
        Pump
    }
    if ($r.P.ExitCode -ne 0) { Fail (T 'A compilação falhou. Abra o registro e mande num issue no GitHub.' 'The build failed. Open the log and post it in a GitHub issue.') }
    $built = Join-Path $Source "out\build\release\$ExeName"
    $dest = Join-Path $Root $ExeName
    # o jogo aberto trava o exe: espera fechar
    while ($true) {
        try { Copy-Item $built $dest -Force; break }
        catch {
            [Windows.Forms.MessageBox]::Show($form, (T 'Feche o jogo para terminar a instalação.' 'Close the game to finish the install.'), $form.Text) | Out-Null
        }
    }
    $bar.Value = 1000
}

# O pack fica fora de source: atualizar o código e recompilar nunca o apaga.
function Step-HdTextures {
    if (-not $hdCheck.Checked -or $Update) { return }
    $tool = Join-Path $Source 'out\build\release\defjam_hdpack.exe'
    if (-not (Test-Path $tool)) {
        Fail (T 'A ferramenta de texturas HD não foi compilada. Veja o registro.' 'The HD texture tool was not built. See the log.')
    }
    $pack = Join-Path $Root 'hd'
    Status (T 'Gerando texturas HD da sua ISO (alguns minutos)...' 'Generating HD textures from your ISO (a few minutes)...')
    $bar.Value = 0; $bar.Style = 'Marquee'
    $r = New-Object DjProc
    $r.Start($tool, "--iso `"$script:HdIso`" --output `"$pack`" --scale 4", $Root)
    $start = Get-Date
    $line = $null
    function Read-HdProgress([string]$line) {
        Add-Content -Path $LogFile -Value $line -Encoding UTF8
        if ($line -match '^progress\s+(\d+)\s+(\d+)') {
            $done = [long]$Matches[1]; $total = [long]$Matches[2]
            $bar.Style = 'Continuous'
            if ($total -gt 0) { $bar.Value = [Math]::Min(1000, [int](1000.0 * $done / $total)) }
            $status.Text = (T 'Texturas HD' 'HD textures') + " $done/$total - " + ('{0:hh\:mm\:ss}' -f ((Get-Date) - $start))
            # A retomada pode enfileirar milhares de linhas: continue pintando a janela durante a drenagem.
            if (($done % 32) -eq 0) { [Windows.Forms.Application]::DoEvents() }
        } elseif ($line -notmatch '^\s*$') {
            $logBox.AppendText($line + "`r`n")
        }
    }
    while (-not $r.P.HasExited) {
        while ($r.Lines.TryDequeue([ref]$line)) { Read-HdProgress $line }
        Pump
    }
    # WaitForExit também esvazia os eventos assíncronos de stdout/stderr antes da última leitura.
    $r.P.WaitForExit()
    while ($r.Lines.TryDequeue([ref]$line)) { Read-HdProgress $line }
    $bar.Style = 'Continuous'
    if ($r.P.ExitCode -ne 0) {
        Fail (T 'Não consegui gerar o pacote HD. Os arquivos já gerados foram mantidos; veja o registro e tente de novo.' 'Could not generate the HD pack. Existing generated files were kept; see the log and try again.')
    }
    # Só habilita depois de gerar com sucesso; preserva todas as outras preferências.
    $settings = Join-Path $Root 'settings.toml'
    $config = if (Test-Path $settings) { [IO.File]::ReadAllText($settings) } else { '' }
    # A preferência é da tabela raiz; não substitui uma chave homônima em seções do jogador.
    $table = [regex]::Match($config, '(?m)^[ \t]*\[')
    $rootLength = if ($table.Success) { $table.Index } else { $config.Length }
    $rootConfig = $config.Substring(0, $rootLength)
    $pattern = '(?m)^[ \t]*hd_textures[ \t]*=[^\r\n]*'
    if ([regex]::IsMatch($rootConfig, $pattern)) {
        $config = [regex]::Replace($rootConfig, $pattern, 'hd_textures = 1') + $config.Substring($rootLength)
    }
    else { $config = "hd_textures = 1`r`n" + $config }
    [IO.File]::WriteAllText($settings, $config, (New-Object Text.UTF8Encoding($false)))
    $bar.Value = 1000
    Log (T 'Pacote HD gerado e ativado. Atualizações preservam a pasta hd.' 'HD pack generated and enabled. Updates preserve the hd folder.')
}

function Step-Shortcuts {
    $sh = New-Object -ComObject WScript.Shell
    $targets = @([Environment]::GetFolderPath('Programs'))
    if ($shortcut.Checked) { $targets += [Environment]::GetFolderPath('Desktop') }
    foreach ($dir in $targets) {
        $l = $sh.CreateShortcut((Join-Path $dir 'Def Jam Fight for NY.lnk'))
        $l.TargetPath = Join-Path $Root $ExeName
        $l.WorkingDirectory = $Root
        $l.Save()
    }
}

$go.Add_Click({
    $go.Enabled = $false; $isoBtn.Enabled = $false; $hdCheck.Enabled = $false
    try {
        Log ("Def Jam: Fight for NY - instalacao em {0} ({1})" -f $Root, (Get-Date))
        if (-not $Update) { Step-Game }
        $vs = Step-Tools
        Step-Build $vs
        Step-HdTextures
        if (-not $Update) { Step-Shortcuts }
        Status (T 'Pronto! Pode jogar.' 'Done! Ready to play.')
        $play.Enabled = $true
    } catch {
        if ("$_" -ne 'falhou') { Log "ERRO: $_"; $status.Text = T 'Não deu certo. Veja o registro.' 'It did not work. See the log.' }
        $go.Enabled = $true; $isoBtn.Enabled = -not $Update; $hdCheck.Enabled = -not $Update
    }
})

if ($Iso) { $isoBox.Text = $Iso }
if ($Auto -and -not $Update) { $form.Add_Shown({ $go.PerformClick() }) }
if ($Update) {
    $isoBox.Enabled = $false; $isoBtn.Enabled = $false; $shortcut.Enabled = $false; $hdCheck.Enabled = $false
    $form.Add_Shown({ $go.PerformClick() })
}
[void]$form.ShowDialog()
