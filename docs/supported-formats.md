# Perfil implementado em 0.1.0

| Recurso | Estado e comportamento |
|---|---|
| PDF digital → DOCX / TXT | Implementado para o perfil abaixo |
| xref clássica, objetos/referências com geração | Implementado; offsets e identidade verificados |
| Revisões `Prev` | Implementado; precedência da última revisão e detecção de ciclos |
| Catálogo/árvore de páginas/herança | Resources, MediaBox, CropBox e Rotate |
| Streams por Length direto/indireto | Implementado; erro em truncamento e ciclos |
| Strings literais, hex, nomes com escapes | Implementado; não usa regex para extrair texto |
| Streams sem filtro | Implementado |
| Type1 Helvetica | WinAnsi comum e StandardEncoding limitado a códigos 32–126, com quotes tipográficos |
| Métricas | Avanços Helvetica internos; Widths explícitos validados quando presentes |
| Texto/estado | BT ET Tf Tm Td TD T* Tj TJ Tc Tw Tz TL Ts, aspas simples/duplas |
| Transformações | q Q cm; texto horizontal com escalas positivas alinhadas |
| Cor | g e rg; Tr somente 0 no modo estrito |
| Layout | Uma coluna, espaços/linhas/parágrafos por geometria |
| DOCX | Texto editável, runs com tamanho/cor/Arial, quebra entre páginas; OOXML Transitional |
| ZIP/XML | Escritores próprios ZIP STORE e UTF-8/XML 1.0; checksums CRC32/Adler32 |
| CropBox diferente/Rotate/UserUnit | Rejeita no estrito; avisa aproximação no lossy |
| Operadores desconhecidos/desenhos/Do | Rejeita no estrito; omite com aviso no lossy |
| FlateDecode e todos os filtros não vazios | Não implementados; stream pode ser omitido com aviso no lossy |
| Inline images | Rejeita sempre; não tenta pular bytes binários |
| ToUnicode, Type0/CID, Differences, outras fontes | Rejeita sempre; não inventa mapeamento |
| xref streams, object streams/híbridos | Não implementados; rejeita |
| Criptografia | Rejeita sempre |
| AcroForm, ações, annotations e camadas | Perda declarada; nunca executa conteúdo ativo |
| XML/ZIP/DOCX leitores | Não implementados |
| Colunas, tabelas, imagens, renderização, OCR | Não implementados |
| DOCX → PDF, outros formatos de entrada | Não implementados |

WinAnsi com códigos de controle ou posições indefinidas de Windows-1252 é
rejeitado; 160 e 173 usam os glifos space/hyphen. Não é alegada conformidade
completa de leitura de qualquer versão PDF, nem suporte universal a WinAnsi.
Modo lossy preserva avisos e recursos descartados no relatório e ainda falha
quando a recuperação não é segura ou não sobra nenhuma camada textual.
