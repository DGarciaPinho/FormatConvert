# ForgeConvert

Conversor C++20 com biblioteca reutilizável `forgeconvert` e CLI. Esta primeira
versão converte **PDFs digitais simples para DOCX com texto editável ou TXT UTF-8**.
Parser PDF, execução dos operadores de texto, layout, CRC32, Adler32, ZIP STORE e
XML são implementados neste repositório. A conversão usa exclusivamente C++ e
biblioteca padrão; nenhum subprocesso, serviço ou biblioteca de formatos.

O perfil é restrito: PDF sem criptografia, xref clássica, streams sem filtros,
fonte Type1 Helvetica e texto horizontal em uma coluna. PDFs comuns com
`FlateDecode`, outras fontes ou CMaps ainda são rejeitados. Não há OCR.

## Compilar e testar

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

CMake 3.20+ e compilador C++20 são suficientes para o produto e os testes C++.
Se Python3 estiver disponível, CTest adiciona validação independente do ZIP/XML
com módulos da biblioteca padrão do Python. Se `gs` e `pdftotext` estiverem
instalados, adiciona um teste com produtor de PDF independente e comparação de
texto. Essas ferramentas são **exclusivamente de teste**, nunca usadas pela CLI
ou biblioteca. Não há downloads nem dependências ocultas no build.

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DFORGECONVERT_SANITIZERS=ON
cmake --build build-sanitize -j 4
ctest --test-dir build-sanitize --output-on-failure
```

## Uso

```sh
./build/forgeconvert capabilities
./build/forgeconvert inspect examples/exemplo.pdf
./build/forgeconvert convert examples/exemplo.pdf --to docx --output examples/exemplo.docx
./build/forgeconvert convert examples/exemplo.pdf --to txt --output examples/exemplo.txt
```

`--overwrite` permite substituir uma saída existente; a entrada nunca pode ser
sobrescrita, incluindo aliases e hard links. A saída é publicada somente depois
da conversão, a partir de temporário no mesmo sistema de arquivos. Não são
criados diretórios de destino automaticamente.

O modo padrão é estrito. `--allow-lossy` permite omitir operadores e recursos
detectados com avisos explícitos. Não recupera fontes sem mapeamento, imagens
inline ou conteúdo comprimido. Se não houver texto em todo o documento, retorna
diagnóstico de possível necessidade de OCR. Uma página vazia em documento com
outras páginas textuais é preservada com aviso.

Helvetica é substituída pelo nome Arial no DOCX, sem incorporar fontes; isso é
registrado no relatório. Parágrafos são inferidos por geometria. O DOCX preserva
separações entre páginas de origem, mas não promete paginação visual idêntica.

## Biblioteca

```cpp
#include <forgeconvert/conversion/convert.hpp>
auto result = forgeconvert::convert("entrada.pdf", "saida.docx",
                                   forgeconvert::OutputFormat::docx);
if (!result) {
    // result.error(): código estável, mensagem, byte e página quando disponíveis
}
```

Vincule ao target CMake `forgeconvert`. A biblioteca não depende da CLI.
`Result<T>` usa `std::variant`, compatível com C++20. Também há APIs próprias
para inspecionar PDF, serializar DOCX a partir do modelo e criar ZIP STORE.

Códigos CLI: 0 sucesso; 1 erro interno; 2 argumentos; 3 recurso/formato não
suportado; 4 entrada inválida; 5 I/O; 6 limite excedido.

Detalhes: [arquitetura](docs/architecture.md), [perfil](docs/supported-formats.md),
[limitações](docs/limitations.md), [progresso e evidências](docs/progress.md),
[referências](docs/specification-notes.md).

O escopo original está preservado em `PLANO_CONVERSOR_CPP.md`. Etapas futuras
incluem DEFLATE próprio, CMaps, layout ampliado e outros formatos; não são
capacidades desta versão.
# FormatConvert
