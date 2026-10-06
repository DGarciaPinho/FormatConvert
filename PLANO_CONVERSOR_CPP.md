# Conversor de arquivos em C++ com biblioteca própria

## 1. Instrução principal para o Codex

Você deve desenvolver um projeto real de conversão de documentos em C++, com uma biblioteca própria reutilizável e uma CLI que a utiliza. O primeiro objetivo é converter PDFs digitais simples em arquivos Word `.docx` com texto editável. Toda a lógica de leitura, escrita, interpretação, compressão e reconstrução deve ser implementada neste repositório.

Leia este documento inteiro antes de modificar o projeto. Inspecione o repositório e suas instruções locais, preserve trabalho existente e comece pela primeira etapa incompleta. Faça implementação funcional incremental, com testes e documentação. Não entregue apenas scaffolding, funções vazias ou um plano. Não diga que há suporte a um recurso apenas porque existe uma classe para ele.

Nome provisório: **ForgeConvert**. Biblioteca: **forgeconvert**. Namespace: `forgeconvert`. C++20, CMake e biblioteca padrão. Primeira plataforma: Linux; manter núcleo portável para Windows.

## 2. O significado de “tudo próprio”

Permitido:
- Biblioteca padrão de C++, compilador, linker, CMake, CTest e recursos básicos do sistema operacional para arquivos.
- Ler especificações públicas dos formatos e algoritmos e implementar o comportamento descrito com código próprio.
- Usar programas externos somente na validação manual ou diferencial, nunca como parte da conversão.
- Usar documentos de teste com origem e licença documentadas.

Proibido no produto e nas dependências de execução:
- Poppler, MuPDF, PDFium, Ghostscript, LibreOffice/Word automatizados, Pandoc, serviços online ou subprocessos para converter.
- zlib, miniz, libzip, bibliotecas XML/JSON, bibliotecas de fontes, codecs de imagem e motores OCR prontos.
- Copiar implementação de terceiros para dentro do repositório e tratá-la como própria.
- Esconder dependências por wrappers, plugins, downloads automáticos ou fallback.

Compiladores e biblioteca padrão não serão reimplementados. A restrição é sobre os componentes do conversor. Qualquer exceção futura precisa ser autorizada pelo usuário; até lá, implemente internamente ou declare o recurso não suportado.

## 3. Expectativas e fronteiras

PDF geralmente armazena instruções de desenho e posicionamento; DOCX descreve conteúdo e estrutura editável. Reconstruir parágrafos, colunas e tabelas envolve inferência. Não existe garantia geral de recuperar exatamente a estrutura original.

“Word” significa `.docx`, não o antigo `.doc` binário. O produto inicial deve favorecer conteúdo editável. Uma imagem de página dentro de um DOCX não equivale a converter seu texto.

PDF digital e PDF escaneado são casos diferentes. OCR próprio é um projeto posterior substancial; não fingir que extração de texto resolve imagens escaneadas. Sem camada textual detectada, emitir diagnóstico: nenhuma camada de texto detectada; o arquivo pode exigir OCR ou conter texto desenhado como formas.

Esta é uma sequência de versões, não uma promessa de suporte universal. Não estimar um prazo curto para PDF, renderização, fontes, codecs e OCR completos.

## 4. Primeiro resultado de ponta a ponta

Antes de tentar suportar PDFs comuns complexos, entregar:

```bash
forgeconvert convert exemplo.pdf --to docx --output exemplo.docx
forgeconvert inspect exemplo.pdf
forgeconvert capabilities
```

Perfil inicial explícito:
- PDF não criptografado, com objetos indiretos e tabela xref clássica válida.
- Páginas com texto horizontal simples, uma coluna, streams inicialmente sem filtros.
- Fontes simples suportadas com mapeamento conhecido; iniciar por Helvetica/WinAnsi e ampliar por testes.
- Operadores essenciais: `BT`, `ET`, `Tf`, `Tm`, `Td`, `TD`, `T*`, `Tj`, `TJ`, `Tc`, `Tw`, `Tz`, `TL`, `Ts`, além de `q`, `Q`, `cm` quando necessários para transformações.
- Documento Word válido com texto, parágrafos e separações entre páginas. Não garantir mesma paginação visual do PDF.
- ZIP próprio usando método STORE; compressão ZIP não é requisito para criar o primeiro DOCX.
- Fontes, filtros ou operadores relevantes não suportados geram erro claro no modo estrito.

Esse perfil é limitado e pedagógico. A versão seguinte deve implementar DEFLATE/zlib próprios para aceitar streams `FlateDecode`, frequentes em PDFs reais.

## 5. Arquitetura

Pipeline:

`arquivo → detector → leitor do formato → representação intermediária → reconstrução de estrutura → escritor do formato → arquivo validado`

Separar leitura de PDF, interpretação gráfica, recuperação de texto, inferência de layout e serialização DOCX. Nenhum módulo deve fazer tudo.

Estrutura sugerida:

```text
CMakeLists.txt
include/forgeconvert/
  core/       # erros, limites, bytes, diagnósticos
  document/   # modelo intermediário e proveniência
  pdf/        # leitor e extração
  docx/       # escritor e, depois, leitor
  codecs/     # CRC32, Adler32, DEFLATE, zlib, ZIP
  xml/        # escrita e, depois, leitura de XML
  conversion/ # opções, capacidades e pipeline
src/          # mesma divisão de include/
apps/cli/
tests/unit/
tests/integration/
tests/fixtures/
docs/
examples/
```

A biblioteca não pode depender da CLI. Criar targets `forgeconvert` e `forgeconvert_cli`, com executável chamado `forgeconvert`. Nada de globais mutáveis para estado de conversão.

## 6. Contrato público

Projetar APIs pequenas e estáveis. A forma exata pode evoluir, mas deve representar:

```cpp
struct ConversionOptions {
    bool strict = true;
    bool overwrite = false;
    ResourceLimits limits;
};

Result<ConversionReport> convert(
    const std::filesystem::path& input,
    const std::filesystem::path& output,
    OutputFormat format,
    const ConversionOptions& options);
```

`Result<T>` deve ser próprio e compatível com C++20; não usar `std::expected`, que requer C++23. Preferir `std::variant` para armazenar resultado ou erro. Erros esperados não devem encerrar o processo da biblioteca.

`ConversionReport` deve conter páginas processadas, avisos, recursos descartados, substituições de fontes e qualidade declarada. Usar códigos de erro estáveis, posição de bytes e número da página quando disponíveis.

## 7. Modelo intermediário

Manter dois níveis:

1. **Conteúdo posicionado:** páginas, caixas, matrizes afins, fragmentos de texto, identificadores de fonte, tamanho, avanço, espaçamento, cor, orientação e posição na origem.
2. **Estrutura lógica:** blocos, parágrafos, runs, títulos, listas, tabelas e imagens.

Texto normalizado em Unicode; índices e unidades devem ser documentados. Coordenadas do PDF não devem ser confundidas com twips do Word. Definir origem e sentido dos eixos, considerar `MediaBox`, `CropBox`, rotação e transformação.

Preservar associação com página/objeto original para diagnóstico. Não inventar tabelas, títulos ou ordem de leitura sem registrar a heurística utilizada.

## 8. Leitor PDF próprio

### Sintaxe e resolução
- Tokenizador binário: whitespace, comentários, nomes com escapes, números, booleanos, null, strings literais com escapes/aninhamento, strings hexadecimais, arrays e dicionários.
- Objetos indiretos, referências com número e geração, offsets em bytes e resolução com limites contra ciclos.
- Localizar `startxref`, interpretar xref e trailer; seguir `Prev` em atualizações incrementais respeitando revisões.
- Encontrar catálogo, árvore de páginas, herança de recursos e geometria.
- Ler streams por `Length`, inclusive indireto. Não usar busca textual por `endstream` como parser normal.
- Depois: xref streams, object streams e arquivos híbridos. Não alegar suporte só por verificar o número da versão PDF.

### Filtros
- Implementar CRC32 para ZIP, Adler32 para zlib, leitor de bits e Huffman.
- Inflater DEFLATE com blocos stored, fixed e dynamic; validar distâncias, comprimentos e fim de entrada.
- Diferenciar DEFLATE bruto usado no ZIP e wrapper zlib usado por FlateDecode.
- Suportar filtros encadeados; depois ASCIIHexDecode, ASCII85Decode e predictors TIFF/PNG conforme perfil declarado.
- JPEG, JPEG2000, JBIG2 e outros codecs ficam explicitamente fora da primeira versão.

### Texto e fontes
- Executar os operadores, não buscar strings com regex.
- Distinguir códigos da fonte de Unicode. Nunca assumir que os bytes de `Tj` são UTF-8.
- Começar com encodings simples conhecidos; implementar `Differences`, métricas e depois `ToUnicode` CMaps, incluindo `bfchar`, `bfrange` e codespaces.
- Reconhecer fontes compostas Type0/CID como capacidade separada; sem mapeamento válido, falhar ou sinalizar perda.
- Calcular posições com matrizes e avanços, incluindo ajustes de `TJ` e espaçamento.
- Adicionar Form XObjects e recursos locais com limites de recursão antes de alegar extração ampla.

Criptografia, reparo de PDF corrompido, assinaturas, formulários, JavaScript e anexos não fazem parte do MVP. Nunca executar conteúdo ativo.

## 9. Recuperação de layout

Primeiro implementar uma coluna horizontal:
- Agrupar fragmentos em linhas por posição e tolerância proporcional ao tamanho da fonte.
- Ordenar linhas e fragmentos geometricamente, preservando a ordem original como informação auxiliar.
- Inserir espaços com base no avanço; não separar letras arbitrariamente.
- Agrupar linhas em parágrafos por distância, recuo e estilo.
- Tratar hifenização conservadoramente; não remover todo hífen no fim da linha.

Depois ampliar para colunas, cabeçalhos/rodapés e tabelas. Criar fixtures para cada heurística, medir o resultado e permitir ajuste documentado. Limiares são heurísticas, não probabilidades calibradas.

## 10. Escritor DOCX próprio

Implementar ZIP e XML internamente. Para o pacote mínimo, gerar:

```text
[Content_Types].xml
_rels/.rels
word/document.xml
```

Adicionar `word/styles.xml`, `word/_rels/document.xml.rels`, propriedades, numbering e media quando usados; relações e content types devem corresponder exatamente às partes existentes.

- ZIP: CRC32, cabeçalhos locais, diretório central, EOCD, flags e offsets corretos. Inicialmente STORE; rejeitar necessidades de ZIP64 até implementá-lo.
- XML: namespaces, escaping, caracteres válidos e serialização determinística. Usar `xml:space="preserve"` quando necessário.
- WordprocessingML: `w:document`, `w:body`, parágrafos, runs, texto, propriedades e seções; estilos, tamanhos e quebras conforme perfil.
- Documentar uso de OOXML Transitional na versão inicial e gerar relacionamentos compatíveis com esse perfil.
- Não criar `.docx` contendo texto puro e não renomear PDF para DOCX.
- Não incorporar fontes sem avaliar direitos de redistribuição. Preferir nomes de fonte e registrar substituições.

Para um futuro leitor XML próprio: proibir DTD e entidades externas por padrão e validar namespaces. Não tratar regex como parser XML.

## 11. Etapas e critérios de conclusão

| Etapa | Entrega | Critério objetivo |
|---|---|---|
| 0 | Projeto e contratos | Configura, compila e executa testes sem dependências externas |
| 1 | Bytes, checksums, ZIP STORE, XML e DOCX | Gera DOCX de texto conhecido aberto sem reparo em leitor independente |
| 2 | Parser PDF estrutural | Resolve catálogo/páginas e extrai streams do perfil inicial |
| 3 | Texto posicionado | Recupera texto, posições e estilos das fixtures suportadas |
| 4 | PDF → DOCX mínimo | Converte múltiplas páginas com texto editável e diagnóstico |
| 5 | DEFLATE/zlib e FlateDecode | Passa vetores stored/fixed/dynamic e PDFs comprimidos |
| 6 | PDF real ampliado | CMaps, revisões, xref/object streams e Forms testados por recurso |
| 7 | Layout ampliado | Colunas/tabelas e estilos têm fixtures e perdas relatadas |
| 8 | Outros formatos textuais | TXT/Markdown/HTML em subconjuntos publicados, sem prometer round-trip perfeito |
| 9 | DOCX → PDF | Leitor DOCX, métricas, layout, paginação e escritor PDF próprios |
| 10 | Imagens/renderização/OCR | Projetos próprios com escopo e validação separados |

Executar cada etapa antes de avançar. DOCX → PDF não é simplesmente inverter a primeira conversão: exige um motor de layout. Imagens e OCR também não são etapas triviais.

## 12. CLI e comportamento

Comandos:

```bash
forgeconvert capabilities
forgeconvert inspect entrada.pdf
forgeconvert convert entrada.pdf --to txt --output saida.txt
forgeconvert convert entrada.pdf --to docx --output saida.docx
forgeconvert convert entrada.pdf --to docx --output saida.docx --allow-lossy
```

Modo padrão estrito: abortar diante de perdas relevantes detectadas no perfil. `--allow-lossy` pode permitir omissões declaradas, mas deve mostrar avisos e não apresentar resultado como fiel. `capabilities` precisa refletir recursos realmente implementados.

Não sobrescrever sem `--overwrite`. Nunca sobrescrever a entrada. Gravar em temporário no diretório de destino e finalizar por operação apropriada à plataforma; remover temporário após falhas. Garantir que nenhum arquivo parcial pareça uma conversão concluída.

Códigos: 0 sucesso; 1 erro interno; 2 argumentos inválidos; 3 formato/recurso não suportado; 4 entrada inválida; 5 erro de I/O; 6 limite excedido. Avisos de perda devem aparecer no relatório, mesmo quando a saída é gerada.

## 13. Robustez

Tratar todo arquivo como entrada não confiável. Centralizar limites configuráveis: bytes de entrada, bytes descomprimidos por stream e no total, objetos, páginas, profundidade, passos de interpretação, tamanho de strings e taxa de expansão. Escolher valores iniciais explícitos e documentá-los.

Checar overflow antes de somar offsets, multiplicar tamanhos ou alocar. Usar RAII e containers seguros. Detectar referências cíclicas e streams truncados. Não acessar fora do buffer. Não extrair caminhos ZIP fora do destino quando existir leitor ZIP.

Limites de operação determinísticos são obrigatórios; prazo de execução e cancelamento podem complementá-los. O usuário deve receber o limite que foi excedido.

## 14. Testes e evidências

Criar harness próprio simples integrado ao CTest, sem Catch2/GoogleTest, respeitando a regra de dependências.

Testar:
- CRC32 e Adler32 com vetores conhecidos; DEFLATE stored/fixed/dynamic, sobreposição de cópia LZ77, distâncias inválidas e entrada truncada.
- Strings PDF, escapes, referências, Length indireto, revisões, ciclos e números extremos.
- Texto com acentos pelo encoding suportado, `TJ`, linhas, várias páginas e transformações.
- XML escaping, preservação de espaços, relacionamentos DOCX e diretório central ZIP.
- Arquivos sem texto, criptografados ou com fontes/filtros desconhecidos: diagnóstico esperado.
- Arquivos malformados e bombas de expansão: limite e erro sem crash.

Não validar escritor ZIP apenas com leitor ZIP próprio: eles podem compartilhar o mesmo erro. Usar inspeção independente e abertura manual em Word/LibreOffice quando disponíveis, apenas para validar. Documentar se isso não pôde ser feito; não dizer que foi testado.

Usar sanitizers de memória/comportamento indefinido onde disponíveis. Adicionar fuzzing para parsers quando a base estiver funcional; ferramentas de teste não podem entrar nas dependências do produto.

Manter fixtures com licença/origem, resultado esperado e recurso coberto. Incluir algumas produzidas independentemente: testar só PDFs gerados pelo próprio projeto é insuficiente.

## 15. Documentação obrigatória

- `README.md`: objetivo, build, CLI, exemplos e restrição de dependências.
- `docs/architecture.md`: módulos, unidades e invariantes.
- `docs/supported-formats.md`: matriz de recursos suportados, rejeitados e planejados.
- `docs/limitations.md`: perda de layout, fontes e ausência de OCR.
- `docs/progress.md`: etapas concluídas, evidências e próxima tarefa.
- `docs/specification-notes.md`: decisões e seções normativas consultadas.

Evitar descrição comercial de “conversor universal”. Registrar capacidade com precisão.

## 16. Referências técnicas primárias

Consultar as especificações durante a implementação; elas são referências, não dependências. Não inventar detalhes binários nem fazer engenharia baseada apenas em exemplos.

- PDF 1.7 / ISO 32000-1: https://pdfa.org/resource/iso-32000-1/
- PDF 2.0 / ISO 32000-2, para expansão posterior: https://pdfa.org/resource/iso-32000-2/
- ECMA-376, especialmente partes 1, 2 e 4 para DOCX/OPC/Transitional: https://ecma-international.org/publications-and-standards/standards/ecma-376/
- ZIP APPNOTE da PKWARE: https://support.pkware.com/pkzip/appnote
- DEFLATE, RFC 1951: https://www.rfc-editor.org/rfc/rfc1951
- zlib, RFC 1950: https://www.rfc-editor.org/rfc/rfc1950
- XML 1.0: https://www.w3.org/TR/xml/

## 17. Como iniciar esta execução

1. Inspecione o repositório e instruções existentes. Não apague nem substitua trabalho do usuário.
2. Registre este escopo e decisões no projeto.
3. Implemente etapas 0 e 1 de verdade: biblioteca, CLI mínima, checksums, ZIP STORE, escritor XML e geração de DOCX de texto conhecido.
4. Compile e execute os testes. Mostre comando e resultado reais.
5. Continue para etapas 2–4 quando a base passar, buscando a primeira conversão PDF → DOCX funcional.
6. Se o tamanho da execução impedir concluir, deixe a última etapa consistente, documente exatamente o que funciona e qual tarefa falta. Não substitua recursos ausentes por ferramentas externas.

Ao encerrar, informe arquivos alterados, comportamento disponível, testes executados e limitações observadas. O critério de sucesso inicial é uma conversão real de um PDF do perfil declarado para um DOCX válido e editável, usando exclusivamente a biblioteca própria.
