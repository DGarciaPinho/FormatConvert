# Limitações observáveis

- A maioria dos PDFs usa compressão e/ou fontes além de Helvetica. Ainda são
  rejeitados. `FlateDecode` próprio é a próxima etapa; nenhuma ferramenta de
  conversão externa é usada como fallback.
- Texto editável não implica reprodução visual exata. Layout por uma coluna,
  inferência de parágrafos, página Letter, substituição Helvetica → Arial e
  ausência de posicionamento fixo podem mudar recuos, espaçamento e paginação.
- Não detecta todos os casos de múltiplas colunas, texto sobreposto ou desenho
  de tabelas. A geometria pode produzir ordem incorreta para entradas fora do
  perfil. Avisos não são uma medida calibrada de qualidade.
- Caracteres, strings e matrizes têm limites conservadores. Fontes desconhecidas,
  CMaps e transformações não horizontais são erros explícitos.
- `Ts` influencia a baseline extraída; não há propriedade de sobrescrito no
  DOCX. `Tc`, `Tw` e `Tz` influenciam avanços/espaços, mas não são reproduzidos
  como propriedades tipográficas no Word. O relatório declara layout heurístico.
- Não há OCR, imagens nem extração de Form XObjects. PDF sem texto retorna
  mensagem de possível necessidade de OCR/texto em formas. Uma página sem texto
  em um documento textual é preservada com aviso.
- O modo estrito rejeita recursos visuais não implementados quando detectados;
  não é um validador completo de conformidade PDF nem detector universal de perdas.
- Não lê/escreve ZIP64, não comprime DOCX, não incorpora fontes, não lê DOCX,
  não exporta PDF e não faz recuperação de PDFs corrompidos.
- Não há garantia de durabilidade em queda de energia nem cancelamento. Limites
  determinísticos são aplicados, mas contagem de passos não é tempo máximo.
- Linux validado. Núcleo usa biblioteca padrão e C++20; particularidades de
  publicação e validação em Windows ainda precisam de testes próprios.
