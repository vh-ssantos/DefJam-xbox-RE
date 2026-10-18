# Def Jam: Fight for NY — port nativo para PC (versão de Xbox)

Um port **não oficial** do *Def Jam: Fight for NY* de Xbox (2004) para Windows. Não é um emulador: o código do jogo
foi recompilado para rodar direto no PC, com imagem em Direct3D 12, som em XAudio2 e controle via SDL.

Feito com o [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) (TGP482), a mesma ferramenta do port do BLACK.

> [!IMPORTANT]
> **Este repositório não tem o jogo.** Não há ISO, arquivos de dados, músicas, vídeos nem código do jogo aqui. Você
> precisa da **sua própria cópia** do Def Jam: Fight for NY de **Xbox, versão americana (USA)**, em imagem `.iso`.

## O que tem a mais que no Xbox

- **Resolução do seu monitor** (ou qualquer outra), **widescreen** nativo do jogo e **supersampling** até 300%.
- **60 quadros por segundo** cravados.
- **Oclusão de ambiente** (SSAO / GTAO): sombras de contato que dão profundidade à cena.
- **Filtro anisotrópico** até 16x.
- **Menu de opções do PC** dentro do jogo: **F10** no teclado ou **BACK + START** no controle.
- **Cheats do jogo** com um clique (pontos de recompensa e músicas), no menu do PC.
- **Nome do User ID digitado no teclado do PC.**
- **Pular os vídeos de abertura** e **pausar quando a janela perde o foco** (Alt+Tab).
- **Pasta de saves** a um clique, saves como arquivos normais (fáceis de copiar e guardar).
- **Discord**: mostra que você está jogando (no menu, no Battle, no modo história, lutando).
- **Atualização automática**: o jogo avisa quando sai versão nova e se atualiza sozinho.
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
| Modo de tela | Tela cheia, tela cheia sem bordas ou janela |
| Resolução | Todas as que o seu monitor aceita |
| Escala de renderização | 50% a 300%. Acima de 100% é supersampling (imagem mais limpa) |
| Filtro anisotrópico | Texturas nítidas de longe |
| V-Sync | Sincronia vertical |
| Oclusão de ambiente | Desligada, SSAO ou GTAO |
| Mostrar FPS | Contador no canto da tela |
| Pular vídeos de abertura | Vai direto para a tela de título |
| Pausar sem foco | Pausa o jogo e o som no Alt+Tab |
| Cheat | Escolha um código e aperte A |
| Nome do próximo User ID | Digite no teclado do PC; na tela do jogo, digite uma letra qualquer e DONE |
| Pasta de saves | Abre no Explorer |
| Versão | Mostra a versão e atualiza quando houver uma nova |

Tudo fica salvo no `settings.toml` ao lado do exe.

## Requisitos

- Windows 10 ou 11, 64 bits.
- Placa de vídeo com Direct3D 12.
- Uns 3 GB livres para a ISO.

## Compilar (para quem quer mexer no código)

1. Clone este repositório.
2. Extraia a sua `.iso` em `assets` (o `default.xbe` e as pastas do disco). Com o Node.js instalado:
   `node tools/xiso_extract.js "D:/Jogos/DefJam.iso" assets`.
3. Instale o Visual Studio 2022 com **Desenvolvimento para desktop com C++**, **Ferramentas Clang do C++** e
   **Ferramentas CMake do C++**.
4. Rode `build.bat`. A primeira compilação recompila o jogo inteiro e pode levar de 30 a 90 minutos.
5. O jogo sai em `out\build\release\DefJamFFNY.exe`.

O código gerado a partir do jogo fica só na sua máquina (`generated/`, fora do git).

## Créditos e licenças

- [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp), por TGP482 (licença MIT): a ferramenta e o runtime
  (`sdk/`), com ajustes deste projeto marcados com `(DefJam)`.
- Ideias do port do [BLACK](https://github.com/TGP482/BLACK-XBERecomp) (menu de opções, oclusão de ambiente,
  pausar sem foco, pular vídeos), reimplementadas aqui.
- GTAO segundo o artigo de Jimenez et al. (2016) e o [XeGTAO](https://github.com/GameTechDev/XeGTAO) da Intel (MIT).
- *Def Jam: Fight for NY* é da Electronic Arts e da AKI Corporation. Este projeto não tem ligação com elas.

Feito com a ajuda do [Claude Code](https://claude.com/claude-code).
