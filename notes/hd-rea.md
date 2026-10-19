# REA e fontes da implementação HD — 2026-10-19

Este registro separa fatos observados nesta máquina, resultados do REA e referências do outro port. Nenhuma medição publicada pelo outro projeto é uma medição deste port.

## REA utilizado

- Skill `reverse-engineer-anything`, versão 37; CLI público `rea-agents` 6.4.0.
- Launcher público disponível no cache npm; não foi necessário instalar motor de análise ou Java.
- O plano de reparo em `runs/rea/setup-plan.json` foi aplicado: somente o registro REA em `C:\Users\vitch\.codex\config.toml`, com backup `C:\Users\vitch\.codex\config.toml.rea.backup`. Comparação com o backup confirmou o launcher corrigido e timeout de 30 s, preservando outras entradas e o ambiente Java/Ghidra. A skill já alinhada não foi substituída. Resultado em `runs/rea/setup-applied.json`; reconectar o cliente é necessário para expor MCP nesta sessão, e a CLI já funciona.
- Ghidra 12.1.3 já instalado em `E:\Tools\ghidra_12.1.3_PUBLIC`. Existe JDK Oracle 21.0.3 completo, x64, em `C:\Program Files\Java\jdk-21`; `java` e `javac` foram verificados. O diagnóstico com `JAVA_HOME` selecionado apenas no processo e acesso apropriado considera o provider disponível: `runs/rea/doctor-jdk21-unsandboxed.json`. O diagnóstico dentro do sandbox não conseguiu observar o Java filho; isso não significa ausência do JDK.
- `inspect-artifact` em `assets/default.xbe` retornou `target_unavailable`, motivo `unsupported binary format`: `runs/rea/xbe-artifact.json`. Não houve decompilação do XBE pelo REA e nenhuma afirmação sobre seu código depende desse resultado negativo.
- `inspect-pe-resources` no executável local compilado do port teve sucesso, Evidence `ev_cd47637d24d2fa52e259935c239640a191394d6e16dea6d3f6bd67b00a2fe5d5`; inventário estático completo de 9 recursos, incluindo 6 imagens de ícone e um grupo. SHA-256 do executável observado: `045b204d585402992f24c009a2d46245326767b9c0e2ebf20fff93892c7de9ee`. Registro completo em `runs/rea/port-pe-resources.json`. Isso comprova utilização funcional do parser REA, não suporte HD, proveniência visual dos ícones ou execução do jogo.

## Referência upstream (leitura de fontes, não Evidence REA)

Fonte MIT: `vyanhursky/DefJamFFNY-recomp`, arquivos `docs/hd-textures.md`, `docs/hd-texture-workflow.md`, `docs/research/hd-*.md`, `scripts/hd_assets.py`, `scripts/build-hd-pack.py` e `config/hd-runtime-aliases.json`, copiados para `runs/hd-reference/upstream` para consulta local.

- Contêineres BIGF/BIG4, compressão RefPack e diretórios SHPX. SHPX suportado pelo decoder de referência: `0x60` DXT1, `0x61` DXT3, `0x62` DXT5, `0x7b` P8 com paleta BGRA e `0x7d` BGRA. Flag `0x2000` indica swizzle; `0x1000` indica ponteiro indireto para dados. A cadeia de anexos P8 leva ao bloco de paleta `0x2a`. O formato `0x65` permanece explicitamente sem decoder validado.
- Identidade atual: SHA-256 de `XRTEX01\0`, cinco inteiros uint32 little-endian (formato, largura visível, altura, entradas de paleta, largura da linha em texels), bytes completos do nível base e paleta BGRA completa. Endereços não fazem parte da identidade. O descriptor antigo de quatro campos é obsoleto.
- Segundo ID usa formato `0x12`, pixels BGRA decodificados, linhas compactas e nenhuma paleta. Serve para correlação exata de arquivos com observações do renderer, sem confundir um arquivo que foi transformado em memória com o arquivo original.
- A referência inclui 13 aliases de hashes, derivados de matches RGBA exatos num piloto limitado. Eles não comprovam cobertura de todos os estados do nosso renderer. É necessário registrar no teste local quais IDs foram encontrados e efetivamente substituídos.
- O corpus upstream é de 18.971 imagens únicas suportadas, com 24.266 aliases de arquivos e uma imagem `0x65` não resolvida (`assets/misc.viv::C_ENV00A.xsh`, `ec1 `, 32x32). O censo é limitado: fontes, vídeos, primitivas vetoriais, conteúdo dinâmico e sete grandes arquivos de áudio são limites separados.
- O upstream separa políticas de mipmaps por semântica: opacidade usa RGB ponderado pelo alfa; material/dados podem usar canais independentes. Alfa não é sempre transparência (p. ex. atlas do lutador). A nossa escolha de processamento deve ser documentada e revisada nos materiais reais.
- Os tempos de geração (~238–257 s), tamanho (~6,21 GB PNG) e resultados de desempenho publicados são da máquina/caminho upstream. Não reutilizar como se tivessem sido medidos aqui.

## Pontos de integração do SDK (inspeção da fonte local)

Em `sdk/runtime/gpu/d3d12.cpp`, `BindTexture` resolve alvos de render antes de `BindGuestTexture`; preservar essa precedência. `Decoded` recebe pixels constantes, depois de `CreateTexture` criar o recurso com as dimensões originais; esse hook sozinho não permite trocar o tamanho nem os dados pelo pack HD. É necessário um caminho anterior à criação ou um caminho dedicado de recurso HD.

`ReadStagePalette` copia a paleta num buffer de 256 entradas preenchido com zero. A identidade interoperável deve usar o tamanho real (`256 >> size_code`), não inferir 256 entradas a partir desse buffer auxiliar. A assinatura `MemSig` existente amostra recursos grandes: ela é um detector de mudanças, não um hash de identidade para nomes do pack. Calcular SHA-256 completo ao decidir a identidade.

Na vida útil do cache D3D12, recursos usados em comandos já gravados precisam continuar vivos até a fence correspondente. O caminho existente usa `CurrentFrame().garbage`; a evicção não pode liberar imediatamente nem ignorar os quatro estágios do draw. A medida de memória deve explicar se inclui dados decodificados da CPU, upload temporário e alocação GPU.

Este levantamento não executou nem interagiu com o jogo, não alterou saves, não fez capturas e não mediu FPS/VRAM. A comprovação HD requer build e observações locais posteriores, conduzidas pela implementação principal.
