# Def Jam: Fight for NY — port nativo para PC (versão de Xbox)

Um port **não oficial e gratuito** do *Def Jam: Fight for NY* de Xbox (2004) para Windows. Não é um emulador: o
código do jogo foi recompilado para rodar direto no PC, com imagem em Direct3D 12, som em XAudio2 e controle via SDL.

Feito com o [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) (TGP482), a mesma ferramenta do port do BLACK.

> **English speakers:** the port and its menus switch to English automatically on a non-Portuguese Windows (or pick it in the in-game F10 menu). Quick start: download the zip from Releases, put your own Xbox (USA) .iso in the `assets` folder (or open the exe and pick the .iso), and play.

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
- O pacote oficial **nunca vem com o jogo**. Se um "Def Jam PC" vier com a ISO dentro, não é este projeto.
- O port **nunca pede login, senha, cartão, Discord ou e-mail**. Ele só acessa a internet para ver se há versão
  nova, no próprio GitHub (dá para desligar com `check_updates = false` no `settings.toml`).
- **Confira o arquivo:** cada release lista o **SHA-256** do `.zip`. No PowerShell:
  `Get-FileHash DefJam-FFNY-PC-v0.1.0.zip` tem que dar exatamente o mesmo código da página da release.
- O `DefJamFFNY.exe` não é assinado digitalmente (custa caro para um projeto gratuito). O Windows SmartScreen pode
  avisar "aplicativo não reconhecido": clique em **Mais informações → Executar assim mesmo**. Se você baixou de
  outro lugar, **não** faça isso.

## O que tem a mais que no Xbox

- **Resolução do seu monitor** (ou qualquer outra), **widescreen 16:9 sem esticar os lutadores** (câmera corrigida) e **supersampling** até 300%.
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

Escolha **um** dos dois jeitos. Os dois dão no mesmo resultado.

### Jeito 1: colocar a ISO na pasta (o mesmo do port do BLACK)

1. Baixe o `.zip` da [versão mais nova](../../releases/latest) e descompacte numa pasta (ex.: `C:\Jogos\DefJam`).
2. Coloque a sua `.iso` do jogo dentro da pasta `assets` que veio no zip.
3. Abra `DefJamFFNY.exe`.

### Jeito 2: escolher a ISO ao abrir

1. Baixe o `.zip` da [versão mais nova](../../releases/latest) e descompacte numa pasta.
2. Abra `DefJamFFNY.exe`. Ele avisa que não achou o jogo e pergunta se quer **escolher a `.iso`**.
3. Escolha o arquivo. O jogo copia a ISO para a pasta `assets` (aparece a barra de progresso do Windows) e abre.

Nos dois casos, da segunda vez em diante é só abrir o `DefJamFFNY.exe`. Na primeira vez ele cria um
`DefJamFFNY.ico` ao lado do exe (o ícone do jogo, tirado da sua cópia) para você usar num atalho.

> No lugar da ISO também servem os arquivos do disco já extraídos: o `default.xbe` e as pastas dele em `assets`.
> Só a versão **USA** é suportada; com outra versão o jogo avisa e provavelmente não funciona.

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

Teclado e mouse (mapeamento completo) ficam para uma próxima versão.

## Menu de opções do PC (F10 ou BACK + START)

| Opção | O que faz |
|---|---|
| Idioma / Language | Automático (pelo Windows), Português ou English |
| Modo de tela | Tela cheia, tela cheia sem bordas ou janela |
| Resolução | Todas as que o seu monitor aceita |
| Proporção | 16:9 (tela cheia; a câmera 3D é corrigida para não esticar os lutadores, só os menus e o placar 2D ficam esticados) ou 4:3 original (nada esticado, com faixas pretas). Vale ao reiniciar |
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

Tudo fica salvo no `settings.toml` ao lado do exe.

## Requisitos

- Windows 10 ou 11, 64 bits.
- Placa de vídeo com Direct3D 12.
- Uns 3 GB livres para a ISO.

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
- GTAO segundo Jimenez et al. (2016) e o [XeGTAO](https://github.com/GameTechDev/XeGTAO) da Intel; SSAO segundo o
  "Alchemy AO" de McGuire et al. (2011).

Código deste projeto sob licença [MIT](LICENSE) (só o código escrito aqui; não cobre o jogo).

---

Feito por **vh-ssantos** com **Claude** (Anthropic), via [Claude Code](https://claude.com/claude-code).
