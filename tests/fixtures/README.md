# Origem, licença e expectativas

Todas as strings/descrições e fontes de fixtures neste projeto são originais,
criadas para estes testes e dedicadas ao domínio público sob CC0-1.0:
https://creativecommons.org/publicdomain/zero/1.0/ . Não há fontes incorporadas.

| Fixture/fonte | Origem | Resultado esperado |
|---|---|---|
| `examples/exemplo.pdf` | PDF textual construído diretamente conforme o perfil, original | 2 páginas; Olá & <Word>, Texto editavel, Segunda pagina |
| C++ `tests/unit/tests.cpp` | Harness original de bytes/objetos | checksums, sintaxe, escapes, Unicode, métricas, estados, transformações, erros/limites |
| `tests/integration/validate.py` | Gerador Python original, separado do parser C++ | ZIP/XML legíveis independentemente; TXT/DOCX de 2 páginas, revisões e erros |
| `independent.ps` | PostScript original; PDF produzido pelo Ghostscript instalado | 2 páginas com 3 parágrafos; texto igual ao pdftotext |

O teste externo não usa um escritor PDF do ForgeConvert. Opções de geração
estão em `tests/integration/external_fixture.py`; evitam compressão, incorporação
de fontes e CMap porque esses recursos estão fora do perfil inicial. O PDF
resultante é gerado no build, não redistribui código do Ghostscript nem fontes.

As fixtures geradas e resultados ficam em `build/integration` e `build/external`.
Testes C++ usam harness próprio; Python, Ghostscript e pdftotext são validação
opcional e nunca dependências do produto. Ainda falta corpus amplo de PDFs de
outros produtores para etapas posteriores.
