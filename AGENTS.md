# Def Jam: Fight for NY — port do Xbox (XboxXBE-Recomp)

Recompilação estática do `default.xbe` (USA, sha1 4352dc84...) para PC com o XboxXBE-Recomp (TGP482, MIT):
x86 -> C++, D3D8 -> D3D12, DirectSound -> XAudio2. O port de PS2 (E:\Projetos\DefJam-RE) continua separado.

## Regras
- Nunca versionar dados do jogo: `assets/`, `iso/`, `generated/`, `runs/` (capturas) ficam fora do git.
- Testes: `tools/boot.ps1` abre na tela 2 sem roubar o foco e captura só a janela. Nunca mandar teclas para o PC;
  botões só por `DEFJAM_PAD_SCRIPT` (dentro do processo).
- Compilar: `compilar.cmd` (vcvars + LLVM portátil em E:\Tools\llvm + CMake/Ninja do VS). Log em `out\compilar.log`.

## Estado (2026-10-17)
- Boot -> vídeos EA/THX/abertura com som -> título -> menu principal -> Story -> criar User ID. 60 fps.

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
| 0x2593A7 / 0x2593C3 | provável IDirectSoundBuffer Pause / PauseEx | ainda não tratados |
| 0x259347 | provável IDirectSoundBuffer PlayEx | ainda não tratado |
| 0x27CE74 | XPP_GamepadType | push antes de XInputOpen |
