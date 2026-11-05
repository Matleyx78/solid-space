# Sicurezza

## Versioni supportate

Fino alla versione 1.0 riceve le correzioni di sicurezza solo la versione più
recente. Finché non esiste una versione con gli algoritmi geometrici, le
correzioni si fanno su `main` ([docs/ROADMAP.md](docs/ROADMAP.md)).

## Segnalare una vulnerabilità

**Non aprire una issue pubblica.** Usa la segnalazione privata di GitHub: scheda
*Security* del repository → *Report a vulnerability*. La segnalazione la vede solo
il responsabile del progetto.

Scrivi, se puoi:

- la versione o il commit di solid-space, il sistema operativo e il compilatore;
- che cosa succede e che cosa ti saresti aspettato;
- i dati minimi che riproducono il problema, per esempio le curve o i valori
  passati alla funzione;
- le conseguenze che vedi (crash, lettura o scrittura fuori dai limiti, blocco,
  comportamento indefinito).

## Che cosa succede dopo

- Risposta di presa in carico entro 7 giorni.
- Se la vulnerabilità è confermata, la correzione si prepara in privato e si
  pubblica; la segnalazione diventa pubblica, con il tuo nome se lo desideri, solo
  dopo l'uscita della correzione.
- Se non è una vulnerabilità, ti spieghiamo perché e, se è un errore del codice,
  ti chiediamo di aprire una issue normale.

solid-space è un progetto volontario: i tempi di correzione dipendono dalla
gravità e da chi è disponibile, ma ogni segnalazione riceve una risposta.

## Che cosa interessa di più

La libreria riceve numeri e forme da chi la usa, spesso letti da file di altri. I
punti più delicati sono quindi i dati ostili: valori non finiti, enormi o fuori
dal dominio dichiarato nelle intestazioni non devono mai causare un crash o un
comportamento indefinito, e le funzioni devono rispondere con una diagnosi. Più
avanti conteranno anche la lettura dei file STEP nel componente `kernel` e le
dipendenze di terze parti (Open CASCADE e SolveSpace, in `third_party/`).

Una funzione che dà un risultato sbagliato su dati validi è un errore del codice,
non una vulnerabilità: apri una issue normale.
