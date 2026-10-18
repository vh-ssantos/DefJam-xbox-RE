# Instalação e recursos extras: o que os outros ports fazem (2026-10-18)

## Como os ports se instalam

| Port | Como chega no jogador | O que o jogador faz |
|---|---|---|
| **BLACK** (Xbox, mesma ferramenta que a nossa) | Um `.zip` no GitHub Releases com o `.exe` **já compilado** e uma pasta `assets` vazia | Descompacta, põe a **ISO** (ou os arquivos do disco) em `assets` e abre o `.exe` |
| **WWE SvR 2011** (Xbox 360) | Um `.zip` com um **Launcher** (programa com abas) e o jogo já compilado | No Launcher: escolhe a ISO e uma pasta, clica **Install** (ele copia o jogo da ISO), ajusta opções, clica **Play**. O Launcher também **se atualiza sozinho** pelo GitHub |
| **Def Jam Vendetta** (PS2) | Um `.zip` com um app de configurações, Python portátil e o código-fonte | Escolhe a ISO e o app **compila o jogo no PC da pessoa** (instala as ferramentas de C++ da Microsoft na primeira vez) e cria um atalho |
| **Dragon Ball BT3** (PS2 / Android) | PC: script `install.py`. Android: um `.apk` | Escolhe a ISO; confere o checksum (versão USA) e instala. Também tem pacotes de textura e atualização pelo GitHub |

**O padrão comum:** ninguém distribui arquivos do jogo, e todos pedem a **ISO do próprio jogador**. A maioria
entrega o programa **já compilado** (BLACK, SvR); só o Vendetta compila na máquina de cada um, e isso faz a
instalação levar muito mais tempo e precisar de ferramentas grandes.

**Recomendação para o Def Jam (segue o padrão, sem ser o "diferentão"):**
1. **Igual ao BLACK** (é a mesma ferramenta, então é o padrão "da família"): um `.zip` no GitHub Releases com
   `DefJamFFNY.exe` + pasta `assets` vazia + um LEIA-ME curto. O jogador põe a ISO em `assets` e joga. O runtime já
   aceita a ISO direto (não precisa extrair).
2. **Toque do SvR, opcional:** um instalador pequeno (`setup.exe`) que pede a ISO, copia para `assets`, cria o
   atalho na área de trabalho com o ícone e, depois, avisa quando sai versão nova.

**Um detalhe:** hoje o ícone é tirado do `default.xbe` na hora de compilar. Num `.exe` distribuído pronto, o
ícone iria junto (é uma imagem da EA). Opções: desenhar um ícone nosso para a versão pública, ou o instalador
extrair o ícone da ISO do jogador na instalação (como fazemos na compilação).

## Recursos do próprio jogo que dá para aproveitar

Achei no executável:

- **Até 4 jogadores no Battle** ("UP TO 4 PLAYERS"). O port já aceita até 4 controles (Xbox, PlayStation, etc.).
  **Falta testar** com mais de um controle.
- **Som 5.1 de verdade.** O jogo tem as opções Dolby Digital 5.1, DTS 5.1 e Pro Logic II. Hoje eu converto para
  estéreo. Dá para ter **saída 5.1 real** para quem tem home theater ou fone surround.
- **Widescreen nativo** (`gWidescreenMode`): já está ligado.
- **Sistema de cheats** (`SubmitCheatCode`): o jogo tem códigos. Dá para pesquisar a lista e, se quiser, um menu
  "Cheats" nas opções do PC.
- **Modo online (Xbox Live)**: existe no jogo (telas `online/...`), mas os servidores morreram em 2010. Ressuscitar
  seria um projeto grande (servidor próprio). Fica como ideia para o futuro.

## Ideias "bacanas" vistas nos outros ports

- **Pular os vídeos de abertura** (BLACK): uma opção no menu.
- **Pausar quando a janela perde o foco** (BLACK).
- **Contador de FPS** (já temos).
- **Saves como arquivos** fáceis de copiar e fazer backup (já são: `out\build\release\hdd`). Dá para pôr um botão
  "abrir pasta de saves" no menu.
- **Digitar no teclado do PC** onde o jogo mostra o teclado na tela (SvR): útil no nome do User ID.
- **Discord** mostrando "jogando Def Jam: luta em Foundation" (SvR).
- **Pacotes de textura** (BT3): fica para depois do jogo 100%, como combinado.
- **Atualização automática** pelo GitHub (SvR, BT3): junto com o instalador.
