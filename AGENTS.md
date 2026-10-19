# Def Jam: Fight for NY — port do Xbox (XboxXBE-Recomp)

Recompilação estática do `default.xbe` (USA, sha1 4352dc84...) para PC com o XboxXBE-Recomp (TGP482, MIT):
x86 -> C++, D3D8 -> D3D12, DirectSound -> XAudio2. O port de PS2 (E:\Projetos\DefJam-RE) continua separado.

## Regras
- Nunca versionar dados do jogo: `assets/`, `iso/`, `generated/`, `runs/` (capturas) ficam fora do git.
- Testes: `tools/boot.ps1` abre na tela 2 sem roubar o foco e captura só a janela. Nunca mandar teclas para o PC;
  botões só por `DEFJAM_PAD_SCRIPT` (dentro do processo).
- Compilar: `compilar.cmd` (vcvars + LLVM portátil em E:\Tools\llvm + CMake/Ninja do VS). Log em `out\compilar.log`.

## Estado (2026-10-17)
- Boot -> vídeos (START pula) -> título -> autosave YES -> menu -> Battle -> One on One -> escolha de lutador ->
  arena (Foundation) -> LUTA rodando (Blaze x Doc, testado 1:20 sem cair). 60 fps nos menus. Perfil salvo no HD virtual.
- História: perfil -> cutscene de abertura inteira -> Fighter Decision -> Sketch Artist (voz) -> cutscene do D-Mob ->
  Choose Style (vídeo na tela de projeção) -> cena da academia -> 1ª luta (tutorial x House): socos, chutes e golpes
  fortes acertam e o tutorial avança.
- **Travamentos intermitentes:** (1) o YES do autosave "não andava": com toques de 0.4 s passou 5/5 (antes ~2 falhas
  em 5 com 0.2 s) -> eram toques perdidos. (2) Uma vez a 1ª cutscene da história congelou num close (o laço
  principal continuava apresentando). Corrigido um suspeito: o SetTimerCallback do host recusava tempo no passado e o
  relógio periódico do jogo (0xD7DE0 arma anterior+período) morreria se a thread atrasasse um período; agora dispara já.
  Log "defjam: timer pedido ... no passado" (DEFJAM_LOG=1) mostra se isso acontece. Ainda não reproduzido de novo.
- Direções seguradas 0.4 s repetem nos menus (anda 2 casas): use "RIGHT 0.15". Toques de 0.2 s às vezes se perdem (telas com vídeo/diálogo); o padrão do live.ps1 agora é 0.4 s.

## Imagem, widescreen e ícone (2026-10-18)
- settings.toml (do jogador): borderless na resolução do monitor, aniso 16. Testes (DEFJAM_WINDOW_MONITOR) usam
  settings-teste.toml (janela 1280x720), para não mexer no do jogador.
- render_percent = % da altura da saída (225 = supersampling 1620p numa janela de 720p): 60 fps cravados na RX 7800 XT.
- Widescreen é nativo do jogo: com saída >= 3:2 o runtime reporta TV widescreen e o jogo abre a câmera (testado 16:9
  e 4:3, proporções corretas nos dois).
- Menu de opções (src/common/menu.cpp): F10 ou BACK+START. Modo de tela, resolução (lista do monitor), escala de
  renderização 50-300%, aniso, V-Sync, contador de FPS; vale na hora e salva. Navegação própria alimentada pelo
  gancho do controle (qualquer controle) e teclado; com o menu aberto o jogo não vê o controle (e só volta a ver
  depois de soltar tudo). Testável pelo live.ps1: "BACK+START 0.3".
- Ícone: tools/make_icon.js tira o ícone oficial do dashboard ($XTIMAGE, XPR0 DXT1 128x128) do default.xbe e o
  CMake embute num .rc gerado na pasta de build (nada do jogo vai para o git).

## Pular abertura e pausar sem foco (src/common/menu.cpp)
- skip_videos (0 não, 1 só logos = padrão, 2 logos e filme): hook em 0x1E460 (define o nome do vídeo, this+841);
  movies/eagames e movies/thx são "logo", movies/intro é o filme; o PadHook aperta START 0.5 s depois. Título em
  ~18 s. settings-teste.toml usa skip_videos = 0 (roteiros).
- Pausa sem foco só na tela cheia EXCLUSIVA (janela e sem bordas seguem rodando, pedido do usuário).
- pause_on_focus_loss: PadHook (thread do jogo) espera enquanto a janela não tem foco; audio::g_paused (novo, SDK)
  faz o mixer entregar silêncio sem avançar as vozes. Desligado quando DEFJAM_WINDOW_MONITOR existe (testes).

## Oclusão de ambiente (src/common/occlusion.cpp)
- gpu::Extension do runtime (sem mexer no backend, só um gancho novo BeforeClear em ext.h/d3d12.cpp). Shaders
  SSAO/GTAO do BLACK, passes via Context::Pass (8 constantes), cena lida por Feedback e multiplicada.
- Momento: o Def Jam limpa a PROFUNDIDADE antes do HUD (no Xbox D3DCLEAR_ZBUFFER = 0x1, TARGET = 0xF0!). A oclusão
  roda nesse clear se a cena 3D (desenhos com zwrite) foi para o mesmo alvo.
- Câmera: matriz de projeção achada varrendo a RAM (formato OpenGL: m11 = -1, m10 = m14 = -1.00002, perto 1, longe
  50000; unidades ~cm). Endereço do objeto câmera (ex. 812B9BC0) guardado e conferido; procura de novo se sumir.
  distância = m14 / (profundidade - m11*m10). Raio padrão 40 (DEFJAM_AO_RADIUS).
- Depuração: DEFJAM_AO_SHOW=1 (só a oclusão), DEFJAM_AO_SHOW=z (profundidade). Menu: Desligada/SSAO/GTAO.
- 60 fps com GTAO na luta (RX 7800 XT).

## Diagnóstico
- **Teste ao vivo (o melhor):** tools/live.ps1 start | press "A" "DOWN 0.3" ... | shot <nome> | stop. O jogo lê os
  botões de runs/pad_cmd.txt (DEFJAM_PAD_CMD), então dá para olhar a tela e decidir o próximo botão. START pula os
  vídeos de abertura (título em ~25 s). Caminho até a luta: START (título), A (aviso), A (YES), RIGHT+A (Battle),
  A (One on One), A (regras), START, A (sem User ID), A (continuar), A (P1), RIGHT+A (CPU), A (arena).
- Scripts por tempo (DEFJAM_PAD_SCRIPT) são frágeis: o tempo de carga varia e o mesmo botão cai em outra tela.
- DEFJAM_LOG=1: mensagens informativas no Release (deixa o jogo ~10 s mais lento até o título: ajuste os scripts).
- DEFJAM_KTRACE=1: cada chamada ao kernel (3 primeiras por local de chamada, depois 1 a cada 1000).
- DEFJAM_CAPTURE_AT=<s>,... + XBE_CAPTURE_DIR=<pasta>: captura completa do quadro (cada desenho, texturas, shaders,
  frame.txt com formato de textura e estados). Foi assim que apareceu o P8 sem paleta.
- tools/dump_process.ps1 + tools/dump_threads.js: onde cada thread está num processo travado.
- O controle real soma botões ao script (XInput lê sem foco): ninguém mexe no controle durante testes.

## Como achar funções do XDK que as assinaturas não pegaram
- Crash: o log mostra o RIP do host; `node tools/dump_symbolize.js <dmp> DefJamFFNY.exe <rip>` dá `sub_XXXXXXXX`
  (o endereço do guest). `node tools/xbe_disasm.js assets/default.xbe <va> <bytes em decimal>` desmonta.
- Este XBE não tem seção .XTLID; as assinaturas sem hash (só xtlid) ficam 0 no config e o código original roda
  em objetos do host -> ponteiro nulo. Identificar pela vizinhança/ordem da vtable (`tools/dsound_thunks.js`)
  e pelo `ret N` (aridade), depois pôr o endereço em `[symbols]`.
- Função que o runtime não tem: hook em `src/game/hooks.cpp` (`X86_FUNC(sub_XXXXXXXX)` + `kernel::Return`).

## Endereços achados à mão
| Endereço | Função | Como |
|---|---|---|
| 0x219B90 | D3DDevice_SetBackBufferScale | grava device+0x96C; hook só guarda |
| 0x216410 | D3DDevice_SetTimerCallback | rdtsc + alarme PTIMER; hook com thread do host (relógio do jogo, callback 0xD7EA0) |
| 0x21E9F0 / 0x21EA10 | D3DDevice_BeginPush / EndPush | config |
| 0x216490 | D3DDevice_FlushVertexCache | método 0x1710, len 0x27 |
| 0x2590D1 / 0x2590F8 / 0x259149 | IDirectSound DownloadEffectsImage / GetEffectData / CommitEffectData | config |
| 0x25927B / 0x259297 / 0x2592B3 | IDirectSoundBuffer SetPitch / SetLFO / SetEG | no-op em hooks.cpp |
| 0x2592CF / 0x259383 | IDirectSoundBuffer SetFilter / StopEx | config |
| 0x2593A7 / 0x2593C3 | IDirectSoundBuffer Pause / PauseEx | hooks.cpp: Stop + Play retomando (nunca chamados até a 1ª luta da história) |
| 0x259347 | IDirectSoundBuffer PlayEx | hooks.cpp: Play do host |
| 0x27CE74 | XPP_GamepadType | push antes de XInputOpen |
| 0x216140 | D3DDevice_SetPalette | grava device+0xF98+estágio*4; sem ele textos P8 (0x0B) ficavam com fundo preto |

## Som: efeitos e falas mudos (RESOLVIDO pelo downmix 5.1, 2026-10-18)
- O usuário confirmou no Battle: golpes, plateia, narrador e falas voltaram com o downmix (estavam no central/
  traseiros). Desempenho medido depois (XBE_PERF=1 + DEFJAM_LOG=1): 60.0 fps em menus e luta, ~1 ms/quadro de CPU.
- Música OK; golpes, provocações e falas não tocam (relato do usuário).
- Arquitetura: motor AEMS da EA. Música = stream mixado em software num buffer DSound de **6 canais** (PCM 48 kHz,
  B0165A00, loop de 0x7080 bytes, o jogo lê GetCurrentPosition). Há também 180 vozes DSound mono **Xbox ADPCM**
  (criadas por 0x123D70) que nunca recebem SetBufferData/Play.
- Corrigido no caminho: o runtime só mixava os canais 0 e 1 de cada voz (Levels), descartando central/LFE/traseiros
  do buffer 5.1. Agora há downmix (dsound.cpp, Render). Medido: os traseiros têm ~35% do sinal; o central fica ~0.
- O que já se sabe: 0x12AB40 (inicia voz de hardware, só se byte [canal+37] & 4) e 0x1297DA (stream) chamam
  SetBufferData/Play; os canais de música 0xBA.. passam por 0x128A60/0x124B50/0x124990. Ao mexer no menu, nenhum
  canal de efeito é montado: o pedido de efeito não chega ao AEMS -> procurar no sistema de eventos
  (sfx/defjam2_eventsys.csi carregado em 0x3CA76 -> 0x2A240) e nos bancos .abk.
- **Atualização:** com trace por contagem, cada movimento de cursor chama 0x12AB40 (caminho software, 0x12ADB4 ->
  0x128A60/0x124B50/0x124CE0) e o mixer 0x12B2F0 dobra; o meter mostrou FR subindo 0.10 -> 0.135 durante os
  movimentos. **Os efeitos de menu tocam.** O silêncio é na luta (bancos por arena, ex. BsmntSFX.abk, carregados
  na hora) e nas falas (stream). Próximo: medir dentro da luta e seguir o carregamento assíncrono dos bancos.
- Ferramentas: DEFJAM_DSTRACE=1 (chamadas DSound + energia por canal a cada 5 s), [xbe] trace no config com
  DEFJAM_TRACE_COUNT=1 (só contagem por 5 s, para traçar regiões inteiras), DEFJAM_KTRACE=1 (4 argumentos).

## Fila de trabalho (combinada com o usuário em 2026-10-18)
1. Cobrir o resto do jogo (modos, arenas, história) + relatos do usuário (Pendente teste do usuário.md no DefJam-RE).
2. ~~Oclusão de ambiente~~ FEITA (2026-10-18), ver seção abaixo. Referência: BLACK src/d3d/d3d12.cpp (Occlusion, ~500 linhas
   de backend + shaders) e src/game/ambient_occlusion.cpp. No Def Jam falta achar: o ponto do quadro entre o opaco e
   o transparente (onde chamar o passe), a matriz de projeção da câmera e o formato do depth.
3. Mapeamento de teclado (próxima leva, antes do instalador).
4. Instalador (setup.exe + ícone): decidir exe pronto x compilar a partir da ISO do usuário.
5. **Depois do jogo 100%:** texturas HD. Sistema de troca de texturas por hash (dump + pasta de substitutas no
   backend d3d12, que já calcula hash das texturas) e um pacote gerado com upscale por IA, mantido fora do git.

## Corrida entre threads no armazenamento (corrigida 2026-10-18)
- Sintoma: crash intermitente no início da luta (~1 em cada 4-11 idas), guest 0x1328C6 lendo [[0x3AD928]+0x38] com o
  global já zerado.
- Causa: o subsistema de dispositivos de save (0x130F00-0x133D00) usa um "trava caseira" não atômica ([0x3AD938] =
  ocupado, [0x3AD928] = dispositivo atual, zerados na saída). No Xbox de 1 núcleo as threads não se cruzavam; no
  runtime cada guest thread roda em paralelo de verdade (prioridades ignoradas).
- Correção (src/game/hooks.cpp): trava recursiva única nas 27 entradas chamadas de fora (DEFJAM_STORAGE_SERIAL) e
  try_lock em 0x133C00, que é chamada pelos callbacks 0x133CB0/0x133CC0 guardados pelo sistema de streaming
  (0xBF2B0) e executados na thread dele. Travar ali (esperar) dava deadlock: 10/10 congelados no menu. Desistir se
  ocupado é o que o próprio jogo faz. Resultado: 12/12 idas à luta limpas.
- tools/estresse_luta.ps1 -Vezes N: repete a ida à luta e diz se travou ou congelou.

## Cheats, nome pelo teclado do PC, saves dos testes (2026-10-18)
- Cheats (menu F10, "Cheat (A aplica)"): o objeto da tela Extras->Cheats nasce em 0x49F40 (thiscall; guardamos o
  this). O comando de script SubmitCheatCode é 0x49D00(this, args, n), args[0] = código; devolve "TYPE=2&VALUE=100"
  (pontos) ou texto de erro. ApplyPendingCheat (hooks.cpp) chama na thread do jogo, a partir do PadHook, com
  kernel::CurrentContext(). Testado: GETSTUFF -> +100 pontos em Unlock Rewards. Códigos: lista pública (21).
- Nome do User ID: comando de script CreateNewUser = 0x52820(args, n), args[0] = nome (SetNewUserID 0x52D70 NÃO é
  chamado na criação). O nome digitado no menu F10 entra no lugar. A conferência de nome repetido é feita pelo
  Flash com o nome da tela (digite na tela uma letra que não exista). DEFJAM_TEST_NAME simula a digitação.
  Para achar comandos de script: "push handler; push nome" perto do nome; trace por contagem (DEFJAM_TRACE_COUNT=1 +
  [xbe] trace com a faixa 0x40000-0x60000) mostra com * as funções novas na ação.
- DEFJAM_HDD=<pasta> (SDK app/main.cpp): o live.ps1 usa hdd-teste, os testes não mexem nos saves do jogador.

## Distribuição (2026-10-18)
- Repositório público: https://github.com/vh-ssantos/DefJam-xbox-RE (sem nada do jogo; auditado).
- tools/release.ps1 -> dist/DefJam-FFNY-PC-v<versão>.zip: exe compilado com DEFJAM_EMBED_ICON=OFF (sem a arte do
  jogo; o exe tira o ícone do XBE do jogador em tempo de execução, src/common/icon.cpp), assets/ vazia com
  instruções, README, licenças. Testado em pasta limpa com a ISO em assets: título em 1920x1080.
- Instalação: (1) ISO em assets, como o BLACK; (2) abrir sem jogo -> pergunta e copia a ISO (app/game_files.cpp
  PickIso, só quando não acha assets em nenhuma pasta acima do exe).
- Atualização automática (src/common/update.cpp): GitHub Releases /latest de vh-ssantos/DefJam-xbox-RE; tag > versão
  do exe (CMakeLists DEFJAM_VERSION) -> menu "Versão: Nova (A atualiza)": baixa o primeiro .zip da release, renomeia o
  exe em uso para .old e extrai DefJamFFNY.exe com tar.exe. DEFJAM_UPDATE_REPO=dono/repo para testar.
  Para publicar: subir DEFJAM_VERSION, rodar release.ps1 e criar a release com tag vX.Y.Z e o zip anexado.
- Discord (src/common/discord.cpp): IPC local (pipe discord-ipc-N), app ID 1561502123772870717 (kAppId = 0 desliga). Presença por comandos
  de script: StartMainMenu 0x46610, StartBattleMode 0x46410, StartStoryMode 0x46450, StartGame 0x46E40.

## Widescreen sem esticar (v0.1.1)
- Parâmetros da câmera 3D: 0x1A4E0(modo, &fov, &proporção, &perto, &longe), thiscall. Modo 0 = jogo (fov horizontal
  em graus, proporção 4:3 FIXA; xs = 1/tan(fov/2), ys = xs * proporção), modo 1 = 16:9 de cinema (faixas em cima e
  embaixo), modo 2 = proporção calculada. Em saída 16:9 o modo 0 esticava tudo. hooks.cpp: Hor+ (proporção do
  monitor e fov = 2*atan(tan(fov/2)*alvo/4:3)); conferido: xs 2.799, ys 4.976. DEFJAM_NO_CAM_FIX desliga.
- O 2D (menus, placar) continua desenhado em 4:3 e esticado: corrigir é por tela (como o hud.cpp do BLACK).
- Opção "aspect_4x3" (app::g_force_4x3, SDK): Widescreen() falso -> imagem 4:3 com faixas; vale ao reiniciar.
- Áudio: diagnosticado com DEFJAM_DSTRACE (pico, estouros, fila vazia, trechos velhos no buffer 5.1, saltos):
  tudo limpo aqui; limitador suave acima de 0.9 no Render.

## Pendências (atualizado 2026-10-18, depois da v0.1.1 e do feedback do Reddit)
1. Modo "Híbrido" de proporção: quadros só 2D (menus) em 4:3 com faixas, quadros com cena 3D (projeção perspectiva
   da câmera / desenhos com zwrite) em 16:9 Hor+. Decidir no BlitFrame por quadro.
2. README: seção Linux / Steam Deck via Proton (o port do SvR2011 roda assim); pedir testes à comunidade.
3. Online amigo x amigo: o runtime tem System Link pela internet (setting join_ip, feito para o co-op do BLACK).
   No Def Jam os símbolos de rede (XNet*, sockets, XOnline*) estão 0 no config: investigar se o jogo tem modo System
   Link (strings "online/...", XOnlineStartup) e ligar. Host + IP; Radmin VPN/ZeroTier ou porta aberta.
4. HUD/placar da luta sem esticar em 16:9 (estilo src/game/hud.cpp do BLACK).
5. Teclado e mouse. 6. Testar o resto do jogo. 7. Texturas HD (depois do 100%).
- Chiado de áudio relatado: era o fone do usuário (descartado).

## Releases publicadas
- v0.1.0 (2026-10-18): primeira pública. v0.1.1 (2026-10-18): Hor+ 3D, idioma PT/EN (src/common/lang.cpp, automático
  por GetUserDefaultUILanguage, também na janela de escolher a ISO), proporção 4:3, pular vídeos em 3 modos, pausa só
  na tela cheia exclusiva, cheats com respostas claras, limitador suave. SHA-256 do zip v0.1.1: 1F4B3EF0...DEFDB06A.
- Divulgado no Reddit (r/decomps). Feedback: tela esticada (3D resolvido, 2D pendente), primeira tela em português
  (resolvido com o idioma), pedidos de Linux e de online.

## Outro port do mesmo jogo: vyanhursky/DefJamFFNY-recomp (visto em 2026-10-19)
- Também Xbox USA, mas outra ferramenta (xboxrecomp do sp00nz, x86 -> C), começou 2026-10-04, v0.10.2, ~80 estrelas,
  CI e testes. Windows (D3D11), macOS Apple Silicon e Linux/Steam Deck (Vulkan) nativos; instalador que RECOMPILA o
  jogo na máquina do jogador (não distribui exe com código do jogo); widescreen com HUD e menus sem esticar (beta);
  texturas HD 4x geradas localmente; remapeamento de controle e teclado; 4 jogadores locais; overlay F1 e launcher;
  menu de desenvolvedor da EA restaurado; gravador de testes (F9). Sem rede.
- O que só o nosso tem (até agora): oclusão de ambiente SSAO/GTAO, idioma PT-BR, cheats do jogo no menu, nome do
  User ID pelo teclado, Discord, atualização automática dentro do jogo, baixar e jogar sem compilar, D3D12.
- Ideias a considerar dele: HUD/menus sem esticar, texturas HD geradas da própria ISO, menu de dev, Linux nativo.
  Diferencial possível nosso: online (ele não tem rede).
- Atenção: o nosso zip leva um exe com o código do jogo recompilado; o dele evita isso de propósito (compila no
  setup). Avaliar com o usuário se vale mudar a distribuição.

## Widescreen completo (2026-10-19, depois da v0.1.1)
- 2D: o renderer (SDK, marcado DefJam) calcula na CPU onde cada desenho cai: lê os vértices (gpu::ReadVertices),
  roda o programa de vértice só para oPos (gpu::EvalPosition) ou usa a posição XYZRHW; w = 1 em todos = 2D. Chama
  Extension::Place, que devolve escala/deslocamento em x no espaço de recorte (DrawConstants::place, aplicado no fim
  do shader de vértice; o recorte/scissor acompanha). Política em src/common/widescreen.cpp: 2D centralizado (k =
  (4/3)/(16/9)); tela inteira sem textura ou com textura <= 128 fica esticada (escurecer/tingir); na luta (passo
  0x87700 rodou há < 250 ms) peças à esquerda de 300 vão para a borda esquerda e à direita de 340 para a direita;
  3D com viewport menor que a tela (lutador num quadro de menu) é centralizado junto. Custo medido: 60 fps, ~2 ms de
  renderer por quadro na luta. DEFJAM_NO_HUD_FIX desliga; opção hud_widescreen no settings.
- Câmera 0x1A4E0 (this = visão: fov +0xC, x +0x14, y +0x18, w +0x1C, h +0x20; args modo, &fov, &proporção, &y, &h):
  modo 0 tela cheia = Hor+; modo 1 (cenas) = y/h originais (sem faixas, 16:9 de fato); modo 2 em tela cheia =
  proporção / k; sub-visões ficam 4:3. ScreenAspect() = 16:9 (o mesmo do BlitFrame), não a proporção do monitor.
- Fonte do mapa: docs/widescreen do vyanhursky/DefJamFFNY-recomp (mesmo XBE, MIT). Creditado no README.
- Próximo: fundo lateral (hoje preto nos menus; o dele usa desfoque), canvas mais largo (hoje 640*escala esticado).

## Distribuição nova (v0.2.0, 2026-10-19): o jogo é montado no PC do jogador
- Pedido do usuário depois de comparar com o vyanhursky: o zip não leva mais exe (o exe tem o código do jogo
  traduzido). tools/release.ps1 (exige git limpo) -> dist/DefJam-FFNY-PC-v<versão>.zip: Instalar.cmd, README,
  licenças, assets/ com instruções e source/ (git archive do HEAD + deps/ zydis, tomlplusplus, imgui, sdl3 copiados
  de out/build/release/_deps sem .git). ~21 MB.
- setup/instalar.ps1 (WinForms, PT/EN, UTF-8 com BOM para o PowerShell 5): lê o default.xbe da .iso (C# DjIso,
  partições 0 e 0x18300000), confere o SHA-1 da USA, grava source/assets/default.xbe (o runtime prefere o xbe
  extraído em assets/, então o da compilação fica em source/), copia a .iso para <raiz>/assets; acha o VS por
  vswhere com VC.Tools + VC.Llvm.Clang + VC.CMake.Project, ou baixa o vs_BuildTools.exe oficial (confere a assinatura
  da Microsoft) e instala com RunAs; escreve source/out/setup-build.cmd (VsDevCmd + PATH do clang/cmake/ninja do VS +
  FETCHCONTENT_SOURCE_DIR_* para deps/ + DEFJAM_EMBED_ICON=OFF) e mostra o progresso do ninja; copia o exe para a
  raiz; atalhos. Log em <raiz>/logs/instalacao-*.log. -Update (depois da atualização), -Iso/-Auto (sem cliques),
  DEFJAM_SETUP_LLVM (clang portátil; esta máquina não tem o clang do VS).
- update.cpp: baixa o zip, extrai em update-tmp, robocopy source/ por cima de <raiz>/source (o que não veio fica:
  assets/default.xbe, out/), copia Instalar.cmd/README/licenças, roda Instalar.cmd -Update e fecha o jogo.
- Quem tem v0.1.x: o atualizador antigo procura DefJamFFNY.exe no zip e falha ("Falhou ao instalar"); precisa
  baixar a v0.2.0 à mão (dizer nas notas da release).
- Linux/Proton: o instalador não roda lá; montar no Windows e copiar a pasta (README).

## PASSAGEM DE BASTÃO (2026-10-19) — leia isto primeiro
Estado: main tem o widescreen completo + a distribuição nova (DEFJAM_VERSION 0.2.0), tudo testado aqui:
- instalador de ponta a ponta numa pasta limpa (E:TesteSetup, do zip de tools/release.ps1): leu a .iso, SHA-1 ok,
  copiou, compilou (2,5 min do zero com DEFJAM_SETUP_LLVM=E:Toolsllvmin), "Pronto"; o exe montado abre a .iso.
- NÃO publicado ainda. Para publicar a v0.2.0 (só com o OK do usuário, que gosta de testar antes):
  1. git limpo -> tools/release.ps1 -> dist/DefJam-FFNY-PC-v0.2.0.zip (imprime o SHA-256);
  2. gh release create v0.2.0 com o zip e notas PT/EN (modelo em notas-v0.1.1 no histórico: avisar que quem tem
     v0.1.x precisa baixar à mão, porque o atualizador antigo procura o exe no zip; explicar o Instalar.cmd);
  3. remover o exe pronto das releases v0.1.0/v0.1.1 (gh release delete-asset) SÓ com o OK explícito do usuário.
- Pendências, em ordem: (1) texturas HD: Lanczos 4x geradas da ISO do jogador no instalador + troca de textura
  por hash no renderer (docs/hd-textures.md do vyanhursky); (2) teclado e remapeamento; (3) menu de dev da EA
  (achado pelo vyanhursky, docs/developer-menu.md lá); (4) online amigo x amigo (seção Pendências acima; diferencial,
  nenhum port tem); (5) fundo desfocado nas laterais dos menus em 16:9; (6) canvas mais largo (hoje 640*escala
  esticado no 16:9).
- Regras do usuário continuam: pt-BR, nada de teclas no PC (só DEFJAM_PAD_CMD), foto só da janela do port, não
  fechar o jogo dele, commits com Co-Authored-By, avisar antes de publicar.

## Texturas HD (2026-10-19, branch hd-texturas)
- Gerador C++20 `tools/hdpack` (`defjam_hdpack.exe`, compilado pelo CMake junto do port); sem Python, IA ou
  downloads de packs. Lê a ISO do jogador com XDVDFS (partições 0 e 0x18300000), ou `--root` extraído. Saída em
  `<instalação>/hd/`, ignorada pelo git. CLI: `--iso arquivo.iso --output hd --scale 4` (ou 2), `--workers N`,
  `--limit N`/`--select texto` só para diagnóstico, `--self-test` só dados sintéticos.
- Pesquisa/identidade/aliases: [vyanhursky/DefJamFFNY-recomp](https://github.com/vyanhursky/DefJamFFNY-recomp), MIT,
  commit 7abc684a795221504d60fd0e8482bb9545cd17c8; crédito no README e licença completa em `sdk/THIRD_PARTY.txt`.
  BIGF/BIG4, RefPack, SHPX: 0x60/61/62 = DXT1/3/5, 0x7b = P8 + paleta BGRA (bloco anexo 0x2a), 0x7d = BGRA8.
  Flag 0x2000 = Morton, 0x1000 = referência indireta. Um 0x65 (`misc.viv::C_ENV00A.xsh`, 32x32) não tem decoder e
  continua original. Limites: arquivos/descompressão de até 128 MiB, 12 níveis de contêiner; sete arquivos grandes
  de áudio excluídos. Não foram necessários novos endereços/hooks do guest.
- Nome = SHA-256 de `XRTEX01\0` + cinco uint32 LE (formato NV2A, largura, altura, entradas reais da paleta,
  largura da linha em texels) + bytes completos do nível base + paleta BGRA. Sem ponteiros/endereço. Header
  compartilhado `sdk/runtime/gpu/hd_texture_id.h`; BC1/2/3 compartilhado em `hd_bc_decode.h` (expansão por bits).
  Segundo ID = BGRA8 compacto, formato 0x12. Índice TSV mapeia raw/decoded/13 aliases verificados da referência
  para PNGs. Correlação estática não comprova substituição de todos os estados do jogo.
- Lanczos3 separável 4x/2x, RGB premultiplicado pelo alfa para opacidade/UI; nos escopos 3D pesquisados
  v2ip/v2cp/v2bg/icrowd/cincrowd/v2cm, canais independentes preservam RGB mesmo quando alfa é zero (dados de
  material). Política explícita por componentes de arquivos, registrada em `policies.tsv`; se aliases compartilham
  conteúdo e algum é de material, prevalece a preservação. PNG com alfa independente. Recipe v3;
  `textures/lanczos3-material-v3-4x/<id>.png`. PNG escolhido para alfa exato e disco menor que DDS BGRA8+mips; sem
  BC7. Índice só é publicado quando termina; PNG/checksums/índice são substituídos atomicamente. Retomada confere
  SHA-256 e política de alfa; arquivo corrompido ou política alterada é reconstruído. Arquivos sem recibo de uma
  execução interrompida são recalculados.
- Renderer `hd_textures.cpp` + caminho separado em `d3d12.cpp` (mudanças SDK marcadas DefJam): worker lê índice,
  PNG via WIC e gera todos mipmaps segundo `policies.tsv`: canais independentes para material, RGB ponderado
  pelo alfa para opacidade. Política ausente preserva canais; alfa não é sempre opacidade.
  O SHA-256 completo é calculado ao subir o original; o detector de mudanças continua sendo MemSig/DirtyEpoch do
  runtime (MemSig amostra recursos grandes). Não afirma detectar imediatamente toda escrita sem lock do guest.
  Somente texturas 2D normalizadas até 1024x1024 entram; rect/linear, cubos, volumes, YUV, depth e render targets
  preservam o caminho original. Pack ausente/pendente/incompatível volta ao original, sem aguardar arquivo.
- Cache HD LRU de 512 MiB de alocação real D3D12, incluindo recursos aposentados que aguardam fence; nunca
  expulsa recurso usado no quadro atual (protege estágios/draws). Upload limitado a 4 MiB por quadro, em fatias de
  linhas, e avanço a cada Present mesmo após sair da tela. Só mostra HD quando todos os mips terminaram.
  CPU: cache pronto até 128 MiB + uploads pendentes até 128 MiB, mais um PNG em decodificação/temporários e índice;
  não confundir esse limite com memória total do processo. `XBE_PERF=1` + `DEFJAM_LOG=1` registram hits/fallbacks,
  VRAM HD e uso DXGI do processo. Overrides só de teste: `DEFJAM_HD_TEXTURES`, `DEFJAM_HD_DIR`, `DEFJAM_HD_BUDGET_MB`.
- `hd_textures=0` padrão, opção F10 bilíngue exige reiniciar; o renderer fixa a preferência no Init. Instalador
  preserva BOM, caixa HD desmarcada, progresso por stdout; ativa a preferência apenas depois do sucesso. Pack
  fica fora de `source`; `-Update` não gera/apaga o pack nem altera a opção. `-HdTextures` habilita no modo Auto.
- Testes sintéticos: SHA-256/XRTEX01, mutação de paleta, BC1/2/3, BIG, RefPack literal/sobreposto/truncado,
  SHPX Morton, Lanczos alfa, PNG BGRA; teste do Loader (`defjam_hd_loader_test`, target explícito) verifica
  fila/aliases/mips/reload, arquivo corrompido e rejeição de caminhos inválidos. Instalador: parsing/BOM e fluxos
  sucesso/falha/Update com processo simulado, preservando as outras preferências.
- Medições locais finais (RX 7800 XT, 1280x720, escala 100%, GTAO, V-Sync, XBE_PERF=1 + DEFJAM_LOG=1):
  18.971 PNGs = 6.213.100.766 bytes; pack com índices/recibos ~6.223.502.746 bytes (6,22 GB). Geração inicial
  326,768 s (5 min 27 s), 8 workers, pasta extraída; piloto de 32 imagens lidas da ISO gerou os mesmos checksums.
  Verificação integral de hashes/dimensões/índice: PASS, 19,99 s, 39.180 chaves; 7.197 material, 11.774 opacidade.
  `Step-HdTextures` + `DjProc` reais, controles WinForms sem mostrar janela: retomou 18.971/18.971 em 85,47 s,
  barra 1000/Continuous, ativou HD preservando width/language e chaves de outra tabela. Drenagem de progresso
  também processa eventos durante grandes filas, para a janela continuar pintando durante uma retomada.
- Battle Blaze x Doc / Foundation com recipe final: roteiro `tools/ate_luta.ps1` precisou de START adicional aos
  32 s (com log, o START aos 28 s ainda pulava o filme); fotos conferiram menu, seleção e luta. 59 amostras de
  aproximadamente 1 s entre t=115,338 e 173,937: média 60,010 fps, faixa 59,8–60,2; host médio 4,918 ms/quadro.
  Luta seguiu até o placar final sem crash. Referência sem HD: 24 amostras 59,8–60,2, média 60,013 fps; havia
  geração em paralelo nessa referência, portanto não é comparação controlada de custo de CPU.
  VRAM HD máxima registrada 511,7/512 MiB; uso DXGI total do processo até 616,5 MiB (inclui nativo/alvos/buffers).
  130 expulsões LRU no percurso final; recursos continuaram protegidos pelas fences. Primeiro quadro com HD
  aos 0,564 s, cache do Windows quente: é o primeiro Present, não o título nem medição de boot frio.
- Teste adicional com orçamento artificial de 64 MiB: pico registrado 62,8 MiB, milhares de evicções e retorno ao
  original, sem crash/trava; menu continuou em torno de 60 fps. Busca automática encontrou `<raiz>/hd` sem override
  de pasta. F10: opção visível em 720p, muda/salva a preferência, renderer continua com o valor fixado até reiniciar.
  Fotos `runs/hd-before-{menu,selection,fight}.png` e `runs/hd-after-{menu,selection,fight}.png`; menu F10 em
  `runs/hd-options*.png`. Logs/relatório/JSON e verificação em `runs/hd/`; nunca versionar esses dados/imagens.
- Limites ainda necessários: revisar todos os materiais (alfa pode codificar dados, não só opacidade), modos,
  arenas e história; testar instalação gráfica completa com a caixa HD e Linux/Proton. Texturas transformadas pelo
  jogo podem precisar de aliases adicionais. Fontes dinâmicas e vídeos não são HD. Nenhum dado do jogo vai ao git.
- REA 6.4.0 usado por CLI e registro Codex reparado com backup; XBE não reconhecido diretamente pelo parser.
  Evidências e limitações em `notes/hd-rea.md`; resultado PE é inventário estático, não prova visual/execução HD.
