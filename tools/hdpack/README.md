# Gerador local de texturas HD

`defjam_hdpack.exe` lê a ISO do jogador diretamente, sem extrair o jogo no disco,
ou uma pasta já extraída. É C++20, usa apenas a biblioteca padrão e o Windows
Imaging Component (PNG sem perda), e não requer Python, modelos de IA nem GPU.
O CMake principal compila o gerador junto com o port. Também é possível configurar
somente esta pasta com CMake para testar a ferramenta.

```text
defjam_hdpack --iso "jogo.xiso.iso" --output "hd" --scale 4
defjam_hdpack --root "jogo-extraido" --output "hd" --scale 2
defjam_hdpack --self-test
defjam_hdpack --iso "jogo.xiso.iso" --output "runs/hd/pilot" --limit 32
```

As opções adicionais são `--workers 1..16` (padrão metade dos processadores lógicos,
limitado a oito) e `--select texto` (seleciona arquivos pelo caminho, sem diferenciar
maiúsculas). `--limit` e `--select` servem para diagnósticos; deixam um pack parcial.
A saída `progress <concluídas> <total>` permite acompanhar o progresso no instalador.
A linha final `HD summary` informa quantidade, bytes de PNG, segundos, arquivos
grandes ignorados, recursos sem suporte e arquivos reaproveitados.

## Leitura e limites

O leitor XDVDFS aceita XISO e a partição de jogo em `0x18300000` de um dump completo,
como `sdk/runtime/kernel/disc.cpp`. A pesquisa de arquivos é limitada a `.viv`,
`.xsh` e `.mad`. São lidos os containers EA BIGF/BIG4 e RefPack, inclusive aninhados,
com limites de 128 MiB por arquivo/descompressão e 12 níveis de containers.
Sete arquivos grandes de áudio do dump USA ficam fora desse censo.

SHPX aceita DXT1/3/5 (`0x60/61/62`), índices P8 com paleta BGRA anexada (`0x7b`) e
BGRA8 (`0x7d`), lineares ou com ordem Morton quando aplicável. Metadados e referências
são conferidos antes de ler cada trecho. A disposição de mipmaps embutidos não foi
validada e não é adivinhada. O tipo `0x65` (um recurso 32x32 em
`assets/misc.viv::C_ENV00A.xsh`) continua no caminho original. Vídeos, fontes vetoriais,
imagens dinâmicas e render targets não fazem parte do pack.

Na ISO USA utilizada nos testes, o censo encontra 18.971 imagens únicas, como na
referência. Este número mede o censo estático; não prova substituição de todos os
recursos durante o jogo. Alterações feitas pelo jogo na paleta/pixels podem exigir
identidades adicionais. O índice inclui 13 aliases exatos do piloto da referência.

## Ampliação e arquivos

Lanczos3 separável amplia a imagem inteira em 2x ou 4x com bordas limitadas ao
último texel, preservando a disposição de atlas. A receita v3 aplica política explícita:
arquivos `.viv` das famílias pesquisadas `v2ip`, `v2cp`, `v2bg`, `icrowd`, `cincrowd`
e `v2cm` (incluindo variantes `_...`) preservam RGB e alfa como canais independentes.
São atlas 3D cujo alfa pode conter dados de material. O restante, incluindo a UI,
usa RGB premultiplicado pelo alfa e volta a alfa independente na saída PNG,
evitando halos de cores invisíveis. A escolha vem dos componentes do caminho dos
arquivos, não dos valores de alfa nem de nomes embutidos de texturas. Se aliases do
mesmo conteúdo incluem um escopo material, os canais independentes têm prioridade.
Não é uma reconstrução de detalhes: fontes, tatuagens e rostos mantêm o desenho da
ISO. A inspeção real encontrou RGB útil em alfa0 em milhares de recursos de
customização, justificando preservar seus canais. Máscaras de recorte, transparências
e escopos adicionais ainda precisam de avaliação visual; a ferramenta não infere
automaticamente o significado de cada canal.
O runtime gera mipmaps no carregamento em sua thread de trabalho.

O pack usa PNG para preservar canais e evitar o espaço de DDS BGRA8 com mipmaps,
que seria muito maior. Não utiliza BC7 nem uma nova compressão com perda.

```text
hd/index.tsv
hd/pack.txt
hd/policies.tsv
hd/checksums-4x.tsv
hd/textures/lanczos3-material-v3-4x/<source-id>.png
```

`index.tsv` contém `<identidade><TAB><caminho relativo PNG>`, sem cabeçalho.
Identidades originais, identidades decodificadas e aliases apontam para o mesmo
arquivo. As dimensões PNG são as dimensões originais multiplicadas por 2 ou 4.
`policies.tsv` registra `<source-id><TAB>channels|opacity` para auditoria da receita.
Os dois fatores usam subpastas próprias; uma nova geração publica o índice ativo
apenas depois de concluir todos os arquivos. Índices/checksums e PNGs são publicados
por troca atômica. O índice é publicado depois das políticas e checksums. Arquivos
já concluídos são reaproveitados quando tanto o SHA-256 quanto a política de alfa
conferem com o recibo anterior. Isso impede reaproveitar um arquivo gerado com
outra política em um piloto parcial. Uma interrupção preserva o índice anteriormente publicado;
arquivos novos sem recibo são recalculados no próximo ciclo.

## Identidade compartilhada e testes

`sdk/runtime/gpu/hd_texture_id.h` implementa SHA-256 sobre `XRTEX01\0`, cinco valores
uint32 little-endian (formato NV2A, largura, altura, quantidade de cores da paleta,
largura em texels da linha), bytes do nível base na disposição original e paleta
BGRA completa. Endereços e níveis seguintes não entram no hash. Esta é a identidade
atual de cinco campos da referência. A identidade decodificada usa formato `0x12`,
BGRA8 linear compacto e nenhuma paleta. `hd_bc_decode.h` fornece o mesmo decodificador
BC para gerador e runtime, evitando divergência na identidade decodificada.

`--self-test` usa somente dados sintéticos: vetores conhecidos SHA-256 e XRTEX01,
mutações na paleta, cores/alfa de BC1/2/3, BIG, RefPack literal e referência sobreposta,
rejeição de dados truncados, SHPX Morton, halos de alfa em UI, preservação de RGB
em alfa0 para material, escopos de arquivo e PNG com ida e volta BGRA.
Os testes reais compararam os mesmos 32 recursos lidos da ISO e da pasta extraída:
índice e checksums PNG idênticos. Não adicione os resultados ao git.

Em 2026-10-19, a geração completa da receita v3 a partir da pasta extraída USA,
com oito trabalhadores, levou 326,768 s e produziu 18.971 PNGs com 6.213.100.766
bytes (aproximadamente 6,21 GB / 5,79 GiB). Foram 7.197 políticas de material e
11.774 de opacidade, além dos sete áudios grandes ignorados e um tipo `0x65` sem
suporte. A verificação separada de todos os hashes, cabeçalhos, dimensões e caminhos
do índice passou em 19,99 s; o índice possui 39.180 chaves para os 18.971 arquivos.
Os 13 aliases da referência e os 32 recursos do piloto ISO conferem. Estes números
medem esta máquina e não constituem promessa de tempo em outro computador.

O teste de retomada preservou 32/32 arquivos intactos. Alterar deliberadamente uma
política no recibo fez reconstruir somente esse arquivo (31 reaproveitados),
restaurando os mesmos hashes e políticas do piloto ISO. A geração completa foi
feita em pasta nova; a correção posterior da validação de política na retomada não
altera seus pixels. Resultados e relatórios completos permanecem somente em
`runs/hd/`, fora do git.

Pesquisa, leitura EA/SHPX, contrato de identidade e aliases adaptados de
[vyanhursky/DefJamFFNY-recomp](https://github.com/vyanhursky/DefJamFFNY-recomp),
commit `7abc684a795221504d60fd0e8482bb9545cd17c8`, licença MIT, copyright 2026
Vlad Yanhursky. Consulte `sdk/THIRD_PARTY.txt`. O filtro premultiplicado, a saída WIC
e a integração do nosso D3D12 são implementações deste port. Nenhum dado ou arte do
jogo integra essa licença ou deve ser incluído no repositório/distribuição.
