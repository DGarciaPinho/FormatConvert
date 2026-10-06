# Progresso em 2026-10-06

Escopo desta execução: etapas 0–4 do plano original, chegando a uma conversão
real PDF → DOCX de texto editável com biblioteca própria. O diretório inicial
continha apenas `PLANO_CONVERSOR_CPP.md`; foi preservado. Não havia instruções
locais adicionais nas pastas `.agents`/`.codex`, nem repositório Git inicializado.

## Entregas

| Etapa | Estado | Evidência |
|---|---|---|
| 0 — projeto/contratos | Concluída | C++20/CMake, targets separados, Result e erros/limites, harness CTest |
| 1 — ZIP/XML/DOCX | Concluída no perfil mínimo | vetores CRC32/Adler32, testes ZIP/XML independentes, LibreOffice importou/renderizou o DOCX |
| 2 — PDF estrutural | Concluída no perfil clássico | xref/Prev, resolução, páginas/herança, Length indireto, ciclos/entradas malformadas |
| 3 — texto posicionado | Concluída no perfil publicado | Helvetica/WinAnsi, estados, Tj/TJ, matrizes, cores, proveniência e linhas |
| 4 — PDF → DOCX mínimo | Concluída | duas páginas editáveis, TXT, CLI, avisos e publicação sem saída parcial |
| 5 — DEFLATE/zlib | Pendente | filtros comprimidos são rejeitados; Adler32 está pronto |
| 6–10 | Pendentes | somente revisões clássicas da etapa 6 foram antecipadas; não há suporte amplo |

"Concluída" se refere ao perfil publicado, não a conformidade integral dos
formatos. Consulte `supported-formats.md` e `limitations.md` para fronteiras.

## Comandos e resultados reais

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure
# 3/3 testes CTest: unit, independent, external_fixture

cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DFORGECONVERT_SANITIZERS=ON
cmake --build build-sanitize -j 4
ctest --test-dir build-sanitize --output-on-failure
# 3/3 testes com AddressSanitizer + UndefinedBehaviorSanitizer

./build/forgeconvert convert examples/exemplo.pdf --to docx --output examples/exemplo.docx --overwrite
unzip -t examples/exemplo.docx
# três partes OK; nenhum erro no ZIP

libreoffice -env:UserInstallation=file:///tmp/forgeconvert-office-validation --headless --convert-to pdf --outdir build/office examples/exemplo.docx
pdftotext build/office/exemplo.pdf -
pdfinfo build/office/exemplo.pdf
# LibreOffice 26.2.6.3 importou o documento e exportou PDF de duas páginas,
# com Olá & <Word>, Texto editavel e Segunda pagina preservados.
```

ASan/UBSan foram executados fora do sandbox: LeakSanitizer falhou inicialmente
sob ptrace no sandbox e passou após a execução autorizada fora dele. LibreOffice
também precisou executar fora do sandbox. Não houve abertura manual em interface
gráfica nem validação no Microsoft Word; a evidência é importação/renderização
headless sem erro e verificação do conteúdo exportado. Ferramentas externas
foram usadas somente em validação, não no pipeline do produto.

Harness C++ cobre checksums, strings/hex/escapes, XML inválido, ZIP, texto e
matrizes, métricas/estado, streams, recursos não suportados, números extremos,
ciclos e limites. Inclui smoke de fuzzing por 1.000 mutações determinísticas
limitadas; não equivale a campanha contínua de fuzzing.
Python valida ZIP por implementação independente, XML com ElementTree, conteúdo
e quebras, revisões/substituição/liberação de objetos, erros CLI, hard links,
proteção da entrada/saída e limpeza de temporários.
O terceiro teste usa PDF gerado pelo Ghostscript a partir de PostScript original
e compara o texto ao `pdftotext`. Há um produtor independente além das fixtures
sintéticas, mas o corpus ainda é pequeno e restrito deliberadamente ao perfil.

## Próxima tarefa

Implementar leitor de bits/Huffman e DEFLATE próprio (stored/fixed/dynamic),
validando distâncias, cópias sobrepostas, truncamentos e limites; depois wrapper
zlib com Adler32 e FlateDecode. Sem recorrer a zlib/miniz nem conversores externos.
Depois, ampliar fontes/CMaps e revisar contra a íntegra da especificação PDF.
