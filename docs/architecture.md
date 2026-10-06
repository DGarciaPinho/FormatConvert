# Arquitetura e contratos

`arquivo → pdf::Reader → interpret → Document/Fragment → reconstruct → docx::write → publicação`

- `core`: `Result<T>`, códigos de erro e limites por conversão. `Failure` é uma
  exceção interna capturada nas fronteiras públicas; não encerra o processo.
- `pdf/parser`: lexer binário e valores recursivos; nomes, números finitos,
  strings, arrays, dicionários e referências. Posições são offsets em bytes.
- `pdf/reader`: xref, revisões `Prev`, resolução/cache, herança da árvore de
  páginas, limites e streams por `Length`, inclusive indireto. Entradas livres
  da revisão mais recente não ressuscitam objetos antigos.
- `pdf/text`: estado gráfico/textual por documento; matrizes e avanços; códigos
  da fonte são decodificados explicitamente, nunca tratados como UTF-8.
- `document/layout`: ordenação geométrica de glifos, linhas, runs e parágrafos.
  O modelo lógico atual só materializa parágrafos/runs; tabelas, títulos e listas
  não estão implementados. `heuristic=single-column-v1` registra a inferência.
- `codecs`: CRC32 e Adler32 bit a bit; ZIP STORE determinístico com diretório
  central. Ainda não há leitores ZIP, Huffman ou DEFLATE.
- `xml` e `docx`: validação UTF-8/XML 1.0, escaping e pacote OPC de três partes,
  WordprocessingML Transitional. Nenhum parser XML faz parte do produto.
- `conversion`: leitura limitada, relatório, pipeline e saída final atômica.
  A CLI apenas interpreta argumentos e apresenta resultados.

## Unidades

Fragmentos usam pontos do espaço padrão PDF: origem inferior esquerda do
`MediaBox`, eixo X para direita e Y para cima. As coordenadas conservam a origem
declarada no PDF, mesmo se não for `(0,0)`. Matrizes são afins `(a,b,c,d,e,f)`;
texto executado usa composição CTM × matriz de texto e aplica rise/avanços.
Esta versão aceita texto com eixos positivos alinhados; rotação/skew são
rejeitados ao mostrar texto. Rotação de página e CropBox diferente são perdas.

`Fragment` representa um glifo decodificado: texto UTF-8, baseline X/Y, avanço,
tamanho transformado, fonte, RGB e página/objeto/byte/sequência de origem.
UTF-8 não tem índice de glifo implícito: índices de strings C++ são bytes.
O modelo não armazena contornos nem caixas exatas de glifos.
`Run` usa pontos para tamanho e RGB normalizado. DOCX escreve tamanhos em
meios pontos; página Letter com margens de uma polegada (twips). Não usa
coordenadas PDF como twips nem tenta fixar cada glifo visualmente.

## Heurísticas v1

Ordenar Y decrescente, agrupar linhas a até 0,25 do menor tamanho de fonte da
baseline representativa, ordenar X crescente dentro da linha. Lacuna acima de
0,18 do tamanho de fonte adiciona um espaço se os glifos vizinhos não forem
espaços. Linhas viram novo parágrafo por distância acima de 1,6 do maior tamanho,
mudança de recuo acima de um tamanho ou mudança de tamanho acima de um ponto.
Demais linhas são unidas com espaço. Nenhum hífen é removido.
Os fatores são heurísticas, não probabilidades; ficam centralizados em
`src/document/layout.cpp`. Não há detecção geral de colunas ou tabelas.

## Limites iniciais

| Limite | Valor |
|---|---:|
| entrada | 64 MiB |
| stream individual | 16 MiB |
| streams acumulados | 64 MiB |
| entradas xref acumuladas/identificadores | 100.000 |
| páginas | 1.000 |
| profundidade sintática, referências, revisões, pilha gráfica | 64 |
| passos de parser/interpretação | 2.000.000 |
| string/nome/token | 1 MiB |
| saída serializada | 128 MiB |
| taxa de expansão | 100, reservada; compressão não implementada |

Configuráveis pela API `ConversionOptions::limits`; CLI usa os defaults.
Números PDF ficam limitados a magnitude 10¹², matrizes a 10⁹, fonte/transformado
a 1.000 pontos. Não se promete aceitar todo número legal na especificação.
Contagem de passos limita trabalho, mas não é prazo de execução nem limite
exato de RAM; o modelo de glifos usa mais RAM que os bytes originais.

## Publicação

Um diretório temporário com nome aleatório é reservado via `create_directory`
no destino; RAII remove arquivo e diretório após sucesso/falha. Após fechar o
arquivo, sem overwrite utiliza `create_hard_link`, que falha atomicamente se o
destino existir; com overwrite utiliza `rename` (substituição atômica em Linux).
Revalida aliases da entrada antes de publicar. Requer filesystem com hard links
para o modo no-clobber; caso contrário retorna I/O. Não garante persistência
contra queda de energia (`fsync` não implementado), nem segurança em diretório
de destino controlado por outro usuário hostil. Em Windows, `rename` sobre saída
existente pode falhar sem substituí-la; núcleo é C++20 portável, plataforma
validada inicialmente é Linux.
