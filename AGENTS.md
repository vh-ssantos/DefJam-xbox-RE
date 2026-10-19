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
- skip_intros: hook em 0x1E460 (define o nome do vídeo, this+841); para movies/eagames, movies/thx e movies/intro o
  PadHook aperta START 0.5 s depois. Título em ~18 s. settings-teste.toml usa skip_intros = false (roteiros).
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
- Discord (src/common/discord.cpp): IPC local, falta o Application ID (kAppId = 0 desliga). Presença por comandos
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
