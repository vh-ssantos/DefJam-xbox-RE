# Def Jam: Fight for NY — port nativo para PC (versão de Xbox)

Um port **não oficial e gratuito** do *Def Jam: Fight for NY* de Xbox (2004) para Windows. Não é um emulador: o
código do jogo foi recompilado para rodar direto no PC, com imagem em Direct3D 12, som em XAudio2 e controle via SDL.

Feito com o [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) (TGP482), a mesma ferramenta do port do BLACK.

> **English speakers:** the port and its menus switch to English automatically on a non-Portuguese Windows (or pick it in the in-game F10 menu). Quick start: download the zip from Releases, unzip it, run `Instalar.cmd` and pick your own Xbox (USA) .iso. The setup builds the game on your PC from your disc (no game code is downloaded); the first build takes a few minutes and needs Microsoft's free Visual Studio Build Tools, which the setup offers to install. Optional HD textures (4x Lanczos, generated locally from your own ISO, ~6 GB) are a checkbox in the setup. Keyboard and mouse work as player 1 with the same default layout as vyanhursky's port (WASD move, I J K L are the face buttons in the pad's diamond, Q/E triggers, arrows right stick, Enter = START); remap keys and controller buttons in F10 → Keyboard and controller.

> [!IMPORTANT]
> **Este projeto não contém o jogo.** Não há ISO, arquivos de dados, músicas, vídeos, artes nem código original do
> jogo para baixar aqui. Você precisa da **sua própria cópia** do Def Jam: Fight for NY de **Xbox, versão
> americana (USA)**, em imagem `.iso`, feita a partir do seu disco.

## ⚠️ Antes de baixar: cuidado com golpes

Este port é **100% gratuito** e só é distribuído **aqui**:

**https://github.com/vh-ssantos/DefJam-xbox-RE**

- **Nunca pague** por este port. Se alguém estiver vendendo, cobrando "acesso", "versão premium" ou "chave de
  ativação", é golpe.
- O download oficial é **só** a página de [Releases](../../releases) deste repositório. Não baixe de sites, canais,
  grupos ou links encurtados de terceiros: podem ter vírus ou ladrões de conta.
- O pacote oficial **nunca vem com o jogo**, nem com o jogo já compilado: ele traz só o código do port e um instalador
  que monta o jogo no seu PC a partir da **sua** ISO. Se um "Def Jam PC" vier com a ISO ou com um exe pronto para
  jogar, não é este projeto.
- O port **nunca pede login, senha, cartão, Discord ou e-mail**. Ele só acessa a internet para ver se há versão
  nova, no próprio GitHub (dá para desligar com `check_updates = false` no `settings.toml`).
- **Confira o arquivo:** cada release lista o **SHA-256** do `.zip`. No PowerShell:
  `Get-FileHash DefJam-FFNY-PC-v0.4.0.zip` tem que dar exatamente o mesmo código da página da release.
- O instalador é um script que dá para ler (`source\setup\instalar.ps1`): ele só baixa o instalador oficial da
  Microsoft (e confere a assinatura dele antes de abrir). O jogo montado não é assinado digitalmente; o Windows
  SmartScreen pode avisar "aplicativo não reconhecido" na primeira vez.

## O que tem a mais que no Xbox

- **Resolução do seu monitor** (ou qualquer outra) e **supersampling** até 300%.
- **Widescreen 16:9 de verdade**: a luta mostra mais da arena, os **menus e o placar ficam na proporção certa** (nada esticado; na luta o placar de cada jogador vai para a borda dele) e as **cenas da história ocupam a tela toda**, sem as faixas pretas.
- **60 quadros por segundo** cravados.
- **Oclusão de ambiente** (SSAO / GTAO): sombras de contato que dão profundidade à cena.
- **Filtro anisotrópico** até 16x.
- **Menu de opções do PC** dentro do jogo: **F10** no teclado ou **BACK + START** no controle, em **português ou inglês** (automático pelo idioma do Windows).
- **Cheats do próprio jogo** com um clique: pontos de recompensa e músicas extras.
- **Nome do User ID digitado no teclado do PC.**
- **Pular os vídeos de abertura** e **pausar no Alt+Tab** (em tela cheia; em janela o jogo continua rodando).
- **Pasta de saves** a um clique, saves como arquivos normais (fáceis de copiar e guardar).
- **Discord**: mostra que você está jogando (no menu, no Battle, no modo história, lutando).
- **Atualização automática**: o jogo avisa quando sai versão nova e se atualiza sozinho, pelo GitHub.
- Controles de **Xbox, PlayStation e Switch**, com vibração. Até **4 controles** no Battle.

## Instalar

O jogo é **montado no seu PC** a partir da **sua** ISO: o zip não traz nada do jogo, só o código do port. Por isso a
primeira instalação demora um pouco, mas é o jeito mais limpo de distribuir um port (é o mesmo modelo do
[DefJamFFNY-recomp](https://github.com/vyanhursky/DefJamFFNY-recomp)).

1. Baixe o `.zip` da [versão mais nova](../../releases/latest) e descompacte numa pasta com caminho curto e sem
   acentos (ex.: `C:JogosDefJam`).
2. Abra o **`Instalar.cmd`**.
3. Escolha a sua `.iso` (ou coloque-a antes na pasta `assets`, que ele já acha). Ele confere se é a versão USA.
4. Se faltarem as ferramentas gratuitas de compilação da Microsoft (**Visual Studio Build Tools**, com C++, Clang e
   CMake), ele pergunta e instala pelo instalador oficial da Microsoft (o Windows pede permissão de administrador).
5. Ele compila o jogo, com barra de progresso: **uns 3 minutos num PC rápido** (mais num PC simples). Pode usar o PC
   enquanto isso.
6. No fim aparece **Jogar**, e um atalho na área de trabalho e no menu Iniciar.

Espaço: uns 3 GB para a ISO, menos de 1 GB para a compilação e de 5 a 8 GB para as ferramentas da Microsoft (se você
ainda não tiver o Visual Studio). As atualizações (F10 → Versão) baixam só o código novo e recompilam o que mudou.

> No lugar da ISO também serve uma cópia extraída do disco: escolha o `default.xbe` dela.
> Só a versão **USA** é suportada; com outra versão o instalador avisa e o jogo provavelmente não funciona.
> Se algo falhar, o botão **Abrir registro** mostra o que aconteceu: mande esse arquivo num issue.

## Controles

São os do Xbox. No controle de PlayStation: X = A, Círculo = B, Quadrado = X, Triângulo = Y.

| Botão | Na luta |
|---|---|
| Y | Soco |
| X | Chute |
| A | Agarrar |
| L + Y ou X | Golpes fortes |
| START | Pausa |
| BACK + START | Menu de opções do PC |

### Teclado e mouse

O teclado (e o mouse) do PC funcionam como o controle do jogador 1: dá para jogar só neles, ou junto com um
controle. O padrão é o mesmo do port do vyanhursky: as teclas **I J K L formam o losango** dos botões do controle.

| Tecla | Botão do Xbox | Na luta |
|---|---|---|
| W A S D | Analógico esquerdo | Andar (e mexer o cursor nos menus) |
| I | Y | Soco |
| J | X | Chute |
| K, Espaço ou clique esquerdo | A | Agarrar (e confirmar nos menus) |
| L, Shift esquerdo ou clique direito | B | Voltar nos menus |
| Q / E (ou botões laterais do mouse) | Gatilhos L / R | Q + I ou J: golpes fortes |
| U / O | Preto / Branco | |
| Setas | Analógico direito | Golpes Blazin' |
| Teclado numérico 8 4 2 6 | Direcional | |
| Ctrl esquerdo / direito | L3 / R3 (apertar os analógicos) | |
| Enter / Backspace | START / BACK | Pausa |
| F10 | | Menu de opções do PC |

**Trocar teclas e botões:** F10 → **Teclado e controle**. Cada ação aceita até 3 teclas (ou botões do mouse) e
mostra o botão do controle que a faz. Escolha a ação e a coluna, aperte A (ou Enter) e depois a tecla ou o botão
novo; Delete deixa a posição vazia. Se a tecla ou o botão já fazia outra ação, as duas trocam. Ali também dá para
desligar o teclado e restaurar o padrão. Fica salvo no `settings.toml` (seções `[teclado]` e `[controle]`).

## Menu de opções do PC (F10 ou BACK + START)

| Opção | O que faz |
|---|---|
| Idioma / Language | Automático (pelo Windows), Português ou English |
| Modo de tela | Tela cheia, tela cheia sem bordas ou janela |
| Resolução | Todas as que o seu monitor aceita |
| Proporção | 16:9 (luta e cenas na tela toda; menus e placar na proporção certa) ou 4:3 original (com faixas pretas nas laterais). Vale ao reiniciar |
| Escala de renderização | 50% a 300%. Acima de 100% é supersampling (imagem mais limpa) |
| Filtro anisotrópico | Texturas nítidas de longe |
| V-Sync | Sincronia vertical |
| Oclusão de ambiente | Desligada, SSAO ou GTAO |
| Mostrar FPS | Contador no canto da tela |
| Pular vídeos de abertura | Não, só os logos (EA, THX) ou logos e filme |
| Pausar sem foco (tela cheia) | Pausa o jogo e o som no Alt+Tab só na tela cheia exclusiva; em janela ou sem bordas, o jogo segue rodando |
| Desbloquear (Unlock) | Os cheats do jogo: escolha com esquerda/direita e aperte A. Cada um vale uma vez por save |
| Nome do próximo User ID | Digite no teclado do PC; na tela do jogo, digite uma letra qualquer e DONE |
| Pasta de saves | Abre no Explorer |
| Versão | Mostra a versão e atualiza quando houver uma nova |
| Texturas HD | Liga ou desliga as texturas HD (gere o pacote no instalador). Vale ao reiniciar |
| Teclado e controle | Teclas do teclado e botões do controle, um por ação (veja acima) |

Tudo fica salvo no `settings.toml` ao lado do exe.

## Texturas HD (opcional)

No instalador, marque **Gerar texturas HD** para ampliar as texturas da sua própria ISO em 4 vezes com Lanczos,
preservando o desenho original, sem IA. O pacote fica na pasta `hd`, ao lado do jogo, e não é distribuído pelo projeto.
Reserve aproximadamente **6 GB adicionais**; a geração leva alguns minutos e mostra o progresso. A quantidade e o
espaço exatos aparecem no registro da geração. A opção vem desmarcada.

Depois, use **F10 → Texturas HD** e reinicie o jogo para ativar ou desativar. Atualizações preservam `hd`.
Na primeira aparição de uma textura, o original continua visível enquanto a imagem HD é carregada em segundo plano.
O cache HD usa até 512 MiB adicionais de memória da placa de vídeo; se faltar espaço, o jogo usa o original.
Vídeos, fontes dinâmicas e alvos de renderização continuam no caminho original. A revisão de todos os materiais e
modos do jogo continua necessária; o pack não contém desenhos novos nem recupera detalhes ausentes no original.

Quem compila pode gerar também em 2x, após a compilação:

```powershell
.\out\build\release\defjam_hdpack.exe --iso "D:\Jogos\DefJam.iso" --output .\hd --scale 2
```

O gerador é C++ e usa componentes do Windows; não exige Python. Consulte [tools/hdpack](tools/hdpack/README.md)
para formatos, retomada e opções de diagnóstico. Para levar a instalação ao Linux/Proton, copie também a pasta `hd`;
o funcionamento HD nesse ambiente ainda precisa de teste.

## Requisitos

- Windows 10 ou 11, 64 bits.
- Placa de vídeo com Direct3D 12.
- Uns 3 GB livres para a ISO.

### Linux e Steam Deck (experimental)

Ainda não há versão nativa, mas o port deve rodar pelo **Proton** (a camada da Valve que roda jogos de Windows),
que traduz o Direct3D 12 com o vkd3d-proton. Ainda não foi testado pelo autor:

1. Por enquanto o instalador só roda no Windows: monte o jogo num PC com Windows (veja **Instalar**) e copie a
   pasta inteira (com o `DefJamFFNY.exe` e a `assets`) para o Linux. A pasta `source` pode ficar de fora.
2. Na Steam: **Adicionar um jogo → Adicionar um jogo que não é da Steam**, escolha o `DefJamFFNY.exe`.
3. Nas propriedades dele: **Compatibilidade → Forçar uma ferramenta de compatibilidade**, escolha o Proton mais novo.

Se testar, conte como foi num [issue](../../issues) (funcionou? FPS? som? controle?).

## Problemas?

Abra um [issue](../../issues) com o arquivo mais novo da pasta `logs` (ao lado do exe) e, se o jogo fechou sozinho,
o arquivo de `CrashDumps`. Diga em qual tela aconteceu.

## Compilar (para quem quer mexer no código)

1. Clone este repositório.
2. Extraia a sua `.iso` em `assets` (o `default.xbe` e as pastas do disco). Com o Node.js instalado:
   `node tools/xiso_extract.js "D:/Jogos/DefJam.iso" assets`.
3. Instale o Visual Studio 2022 com **Desenvolvimento para desktop com C++**, **Ferramentas Clang do C++** e
   **Ferramentas CMake do C++**.
4. Rode `build.bat`. A primeira compilação recompila o jogo inteiro e pode levar de 30 a 90 minutos.
5. O jogo sai em `out\build\release\DefJamFFNY.exe`.

O código gerado a partir do jogo fica só na sua máquina (`generated/`, fora do git). Os scripts `compilar.cmd`,
`tools/*.ps1` e `tools/*.js` são as ferramentas de desenvolvimento do autor e têm caminhos da máquina dele
(Visual Studio e LLVM no drive E:); quem for usar ajusta para a sua. O `build.bat` é o caminho genérico.
O histórico técnico (o que foi descoberto e corrigido) está em [AGENTS.md](AGENTS.md).

## Aviso legal

Projeto de fã, sem fins lucrativos, feito para preservar e jogar no PC um jogo que você já possui. *Def Jam: Fight
for NY*, seus personagens, músicas e artes são da **Electronic Arts** e da **AKI Corporation**. Este projeto não tem
ligação com elas nem com a Microsoft, e não é endossado por elas. Não distribua o jogo; compre ou use a sua cópia.

## Créditos

- **Autor: [vh-ssantos](https://github.com/vh-ssantos)** — ideia, direção, testes e jogatina.
- [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp), por TGP482 (MIT): a ferramenta e o runtime (`sdk/`),
  com ajustes deste projeto marcados com `(DefJam)`.
- Ideias do port do [BLACK](https://github.com/TGP482/BLACK-XBERecomp) (menu de opções, oclusão de ambiente, pausar
  sem foco, pular vídeos), reimplementadas aqui.
- O mapa do widescreen (câmera, cenas, placar, passo da luta) vem da pesquisa publicada pelo
  [DefJamFFNY-recomp](https://github.com/vyanhursky/DefJamFFNY-recomp) de vyanhursky (MIT), outro port do mesmo jogo:
  vale conhecer, ele roda também no macOS e no Linux.
- O formato de identidade das texturas HD, a pesquisa dos arquivos BIG/RefPack/SHPX e os aliases de correlação vêm
  também de [vyanhursky/DefJamFFNY-recomp](https://github.com/vyanhursky/DefJamFFNY-recomp), MIT, copyright
  Vlad Yanhursky. O gerador C++ e o carregador D3D12 foram implementados para este runtime; licença preservada em
  `sdk/THIRD_PARTY.txt`.
- GTAO segundo Jimenez et al. (2016) e o [XeGTAO](https://github.com/GameTechDev/XeGTAO) da Intel; SSAO segundo o
  "Alchemy AO" de McGuire et al. (2011).

Código deste projeto sob licença [MIT](LICENSE) (só o código escrito aqui; não cobre o jogo).

---

Feito por **vh-ssantos** com **Claude** (Anthropic), via [Claude Code](https://claude.com/claude-code).
Implementação de texturas HD com **Codex** (OpenAI).
