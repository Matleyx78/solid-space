# [X.Y-PN] Titolo

<!--
Modello della scheda di un pacchetto (vedi 0000-modello.md, sezione 6). La scheda
sta in docs/tappe/X.Y/PN-<nome>.md e la issue la cita.

La scheda è autosufficiente: l'agente legge AGENTS.md, questa scheda, gli ADR
che cita e CODING_STYLE.md, e nient'altro della tappa. Quello che gli serve
del piano (regole sui testi, file comuni, decisioni) si ripete qui, in breve.
Le regole per la dimensione sono in LAVORO-IN-PARALLELO.md, "Preparare il piano".
-->

- **Tappa:** X.Y — [piano](../X.Y-titolo.md)
- **Issue:** #…
- **Difficoltà:** facile | media | impegnativa
- **Agente:** claude | codex | gemini | …
- **Modello e impegno:** leggero | medio | forte; impegno basso | medio | alto
  (tabella in LAVORO-IN-PARALLELO.md)
- **Riferimenti:** issue `[X.Y-RN]` (#…), agente … | non servono: motivo
- **Dipende da:** —
- **Componente:** uno solo (più i suoi test e le righe dei file comuni)
- **Da leggere:** gli ADR e i file di codice che servono davvero, con il motivo

## File

L'elenco completo dei file da creare o modificare. Al massimo una quindicina;
se ne servono di più, il pacchetto va diviso.

## Cosa fare

Il lavoro, con le interfacce da rispettare (nomi, firme, comportamenti) scritte
per intero.

## Regole della tappa che valgono qui

Solo quelle che riguardano questo pacchetto: testi e messaggi, file comuni,
decisioni del piano. Se non ce ne sono, si scrive "nessuna".

## Test richiesti

I casi minimi che devono esistere.

## Fatto quando

Condizioni verificabili per considerare il pacchetto concluso. Ognuna deve avere
almeno un caso di verifica nel pacchetto di riferimenti.
