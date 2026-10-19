Você vai implementar TEXTURAS HD no port de PC do Def Jam: Fight for NY (versão de Xbox), no repositório
E:\Projetos\DefJam-xbox-RE (GitHub vh-ssantos/DefJam-xbox-RE). Responda ao usuário em português do Brasil; ele é
iniciante, então explique o que fez em linguagem simples.

## Antes de tudo
1. Leia AGENTS.md inteiro (principalmente "Regras", "Widescreen completo", "Distribuição nova" e "PASSAGEM DE
   BASTÃO"). Ele explica como compilar (compilar.cmd), testar (tools/live.ps1) e o que já existe.
2. Trabalhe SÓ na branch `hd-texturas` (crie a partir de main). Não mexa na main, não faça merge, não publique
   release. Pode dar push da branch. Outro agente (Claude) mexe na main em paralelo; evite reformatar arquivos
   inteiros para não dar conflito.
3. Commits terminam com a linha: Co-Authored-By: Codex (OpenAI) <noreply@openai.com>

## Regras que não podem ser quebradas
- NUNCA versionar nada do jogo: imagens/texturas extraídas ou geradas, a .iso, assets/, generated/, runs/, packs HD.
  Os packs são gerados no PC de cada jogador a partir da .iso dele.
- Nunca mandar teclas/cliques para o PC do usuário. Botões do jogo só por DEFJAM_PAD_CMD (tools/live.ps1 press ...).
  Fotos só da janela do port (tools/live.ps1 shot <nome>).
- Não fechar um jogo que o usuário tenha aberto: confira com Get-Process DefJamFFNY e a hora de início antes.
- Mudanças dentro de sdk/ ficam marcadas com o comentário "(DefJam)".

## O que fazer (o modelo é o do port do vyanhursky, MIT, mesmo jogo e mesmo XBE)
Referência: https://github.com/vyanhursky/DefJamFFNY-recomp — leia docs/hd-textures.md, docs/hd-texture-workflow.md,
docs/research/hd-*.md, scripts/hd_assets.py, scripts/build-hd-pack.py, config/hd-runtime-aliases.json. Ele gera
"faithful HD": cada textura do jogo ampliada 4x com Lanczos (sem IA), ~19 mil imagens, ~6 GB, nomeadas pelo
CONTEÚDO da textura original, e o renderer troca na hora de carregar. Pode reaproveitar ideias e código (MIT, dê
crédito no README e no AGENTS.md), mas o nosso motor é outro: C++ e Direct3D 12 (sdk/runtime/gpu/).

1. Gerador do pack (roda no PC do jogador, chamado pelo instalador):
   - ferramenta C++ nova compilada pelo CMake (ex.: tools/hdpack/), sem Python (o instalador não tem Python).
     Pode usar stb_image / stb_image_write / stb_image_resize2 (domínio público) em sdk/ ou thirdparty/.
   - lê as texturas direto da .iso do jogador (o leitor de ISO do runtime está em sdk/runtime/kernel/disc.cpp)
     ou de uma cópia extraída; descobre os formatos de arquivo com a pesquisa do vyanhursky.
   - amplia com Lanczos (padrão 4x, opção 2x), cuidando do alfa (premultiplicado para não sujar as bordas), e
     grava com nome = hash do conteúdo original (formato + tamanho + bytes), num formato rápido de carregar
     (PNG ou DDS; se escolher DDS com BC7, explique o porquê).
   - saída em <raiz da instalação>\hd\ (fora do git; ver layout em setup/instalar.ps1).
2. Troca no renderer:
   - ao subir uma textura (sdk/runtime/gpu/texture.cpp e d3d12.cpp, procure BindTexture/Decoded; ext.h tem os
     ganchos de extensão), calcula o mesmo hash e, se houver imagem HD, cria o recurso no tamanho HD com mipmaps.
   - texturas lineares/"rect" (coordenadas em texels) e alvos de render ficam como estão.
   - orçamento de memória de vídeo com cache (LRU) e carregamento que não trave o jogo (o jogo precisa continuar
     em 60 fps; meça com XBE_PERF=1 e DEFJAM_LOG=1, linhas "perf:" no log).
3. Opções:
   - settings.toml `hd_textures` (0 desligado / 1 ligado) e uma linha no menu F10 (src/common/menu.cpp, textos
     bilíngues com T("pt", "en")), vale ao reiniciar;
   - no instalador (setup/instalar.ps1, UTF-8 COM BOM, janela WinForms): caixa "Gerar texturas HD (mais ~6 GB e
     alguns minutos)" desmarcada por padrão, com barra de progresso; o modo -Update não pode apagar o pack.
4. Teste e prove:
   - compile com compilar.cmd; teste com tools/ate_luta.ps1 (vai até uma luta) e tools/live.ps1;
   - fotos antes/depois (menu, seleção de lutador, luta) em runs/ (não versionar);
   - meça: tempo de geração, tamanho do pack, VRAM, fps na luta, tempo da primeira tela.
5. Documente:
   - seção nova no AGENTS.md ("Texturas HD") com o que descobriu, formatos, endereços e números medidos;
   - README.md: opção nova, espaço em disco, crédito ao vyanhursky.

## Entrega
Ao terminar: branch `hd-texturas` com push, um resumo para o usuário (o que mudou, números medidos, fotos em runs/,
o que ficou pendente) e diga claramente o que NÃO conseguiu testar. Não faça merge nem release.
