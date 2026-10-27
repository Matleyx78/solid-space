# Tappa X.Y — Titolo

- **Stato:** in programma | aperta | chiusa
- **Componente:** uno o più componenti, oppure "tutti"
- **Milestone GitHub:** `vX.Y`
- **Apertura:** AAAA-MM-GG — **Chiusura:** AAAA-MM-GG

<!--
Una tappa "in programma" compila solo le sezioni 1-4 (la scheda).
All'apertura il responsabile aggiunge le sezioni 5-8 (il piano dettagliato).
La scheda della tappa 0.1 è anteriore a questo modello: si allinea quando se ne
scrive il piano.
-->

## 1. Obiettivo

Cosa sarà possibile fare alla fine della tappa, in una o due frasi, dal punto di
vista di chi usa la libreria (Tecnigrafo o un altro programma).

## 2. Confini

**Dentro:** l'elenco esatto di ciò che la tappa realizza.

**Fuori:** ciò che sembra collegato ma appartiene ad altre tappe (indicare quale).

## 3. Criteri di chiusura

Verificabili, uno per riga, con casella da spuntare alla chiusura.

- [ ] …

## 4. Progetto di massima

Componenti coinvolti, nuove classi principali, ADR da scrivere, rischi.

## 5. Prerequisiti

Tappe chiuse e ADR accettati da cui questa dipende.

## 6. Pacchetti di lavoro

Ogni pacchetto è **una pull request**. I pacchetti senza dipendenze tra loro si
possono fare in parallelo; due pacchetti che toccano gli stessi file sono legati
da "Dipende da".

Un pacchetto che cambia il comportamento geometrico ha anche un pacchetto di
riferimenti `RN`, scritto da un altro agente e unito prima del lavoro
([ADR 0004](../adr/0004-robustezza-e-verifica.md)). La sua issue si cita in
"Dipende da".

Questa sezione contiene **solo la tabella**: ogni pacchetto ha una sua **scheda**
in `docs/tappe/X.Y/PN-<nome>.md` (modello in
[0000-pacchetto.md](0000-pacchetto.md)), e la sua issue la cita. Così un agente
legge la scheda del suo pacchetto e non tutto il piano. Le regole per dividere il
lavoro sono in [LAVORO-IN-PARALLELO.md](../LAVORO-IN-PARALLELO.md), sezione
"Preparare il piano".

| Pacchetto | Scheda | Issue | Dipende da | Riferimenti | Difficoltà | Agente | Modello | Impegno |
|---|---|---|---|---|---|---|---|---|
| P1 Titolo | [P1](X.Y/P1-nome.md) | #… | — | R1 (gemini) / non servono: motivo | facile / media / impegnativa | claude / codex / gemini / … | leggero / medio / forte | basso / medio / alto |

L'**ordine delle righe** è l'ordine in cui gli agenti prendono "il prossimo
pacchetto" (AGENTS.md). Le issue di riferimenti `[X.Y-RN]` hanno una riga
ciascuna, prima del loro pacchetto di lavoro.

**File comuni.** I file che più pacchetti toccano nello stesso periodo (di norma
i `CMakeLists.txt` e `AGENTS.md`): ognuno aggiunge solo le sue righe, in ordine
alfabetico, e chi arriva dopo si aggiorna. Si elencano qui e si ripetono nelle
schede che li toccano.

## 7. Prova manuale

Passi da eseguire a mano prima della chiusura, su ogni sistema operativo che la
CI non copre o che richiede un controllo a occhio.

## 8. Modifiche al piano

Cambiamenti decisi durante la tappa, con data e motivo.
