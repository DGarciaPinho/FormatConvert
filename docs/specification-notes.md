# Referências e decisões normativas

Implementação original; especificações são referências, não dependências.
Não foi incorporado código de bibliotecas de terceiros.

- [ISO 32000-1 / PDF Association](https://pdfa.org/resource/iso-32000-1/):
  seções alvo 7.2–7.3 (sintaxe/objetos), 7.5 (estrutura/xref), 7.7.3 (páginas),
  8.3.4 (matrizes), 9.3–9.4 (estado/operadores de texto), 9.6 e anexo D (fontes).
  A página primária e os trechos indexados do PDF Adobe foram consultados;
  os links Adobe para a íntegra retornaram 404 nesta execução. Esta limitação
  da consulta fica registrada; ampliar conformidade exige consultar a íntegra.
  Fixtures de produtor independente e extração diferencial verificam o perfil
  implementado, mas não substituem revisão normativa completa.
- [ECMA-376](https://ecma-international.org/publications-and-standards/standards/ecma-376/)
  e [white paper oficial, seção 5.6](https://ecma-international.org/wp-content/uploads/OpenXML-White-Paper.pdf):
  pacote mínimo WordprocessingML com content types, relacionamento principal e
  documento. Esta versão usa namespaces Transitional de 2006, principal em
  `word/document.xml`, override do content type e preservação explícita de
  espaços. Não declara validação completa contra todos os schemas ECMA-376.
- [PKWARE APPNOTE 6.3.10](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT),
  4.3.7, 4.3.12 e 4.3.16: cabeçalho local, diretório central e EOCD, campos
  little-endian. Método 0, flag UTF-8, CRC32, data DOS fixa 1980-01-01.
  Sem data descriptor, criptografia ou ZIP64.
- [XML 1.0, quinta edição](https://www.w3.org/TR/xml/), 2.2 e 2.4:
  caracteres Unicode válidos e escaping. Rejeita UTF-8 sobrelongo, surrogates,
  truncamento e controles proibidos. Sem DTD, entidades externas ou leitor XML.
- [Ghostscript, opções do produtor PDF](https://ghostscript.readthedocs.io/en/master/VectorDevices.html):
  consultado exclusivamente para o teste independente. `ToUnicodeForStdEnc=false`
  gera um PDF dentro do perfil sem CMap; não remove recursos do PDF do usuário.

Etapa 5 deverá consultar [RFC 1951](https://www.rfc-editor.org/rfc/rfc1951) e
[RFC 1950](https://www.rfc-editor.org/rfc/rfc1950) para implementar DEFLATE e zlib
internamente, com vetores stored/fixed/dynamic e limites de expansão.
