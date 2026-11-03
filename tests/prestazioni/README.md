# Banco delle prestazioni del piano

`solid_space_banco_prestazioni` usa solo le API pubbliche di `geom2d` e
`robust`, senza librerie di benchmark. Si compila insieme ai test sui tre
sistemi, ma non è registrato in CTest e la CI non lo esegue. Le prestazioni
sono obiettivi da misurare, non requisiti che fanno fallire la CI.

## Compilazione ed esecuzione

Con gli strumenti di [AMBIENTE-SVILUPPO.md](../../docs/AMBIENTE-SVILUPPO.md),
dalla radice del repository, in Bash (anche su Windows):

```sh
export SS_BUILD_SUFFIX=-codex
cmake --preset release
cmake --build --preset release --target solid_space_banco_prestazioni
./build-codex/release/tests/solid_space_banco_prestazioni
```

Su Windows l'ultimo comando termina con `.exe`. Per la sola geometria del
piano si può configurare con `cmake --preset release
-DSOLID_SPACE_BUILD_KERNEL=OFF`, senza OCCT. Il banco non richiede il kernel.
Usare sempre una build Release: tempi Debug o con sanitizzatori non sono
confrontabili con gli obiettivi degli ADR.

L'uscita è una tabella Markdown pronta per le note di rilascio. Accompagnarla
con commit, sistema operativo, processore, compilatore e opzioni di build.
Eseguire a macchina scarica; il tempo peggiore risente anche delle pause del
sistema operativo. Una macchina virtuale condivisa non sostituisce la prova
sul computer del responsabile prevista dal piano della tappa.

## Dati e misure

Il generatore parte dal seme `0x51717`: usa un LCG a 32 bit senza segno e
coordinate multiple esatte di 1/4. Non usa distribuzioni della libreria standard,
trigonometria di sistema, orologio o entropia per generare i dati. I casi al
limite e le dimensioni delle polilinee sono fissati nel sorgente. Stessi dati
su tutti i sistemi; i tempi, naturalmente, cambiano.

Ogni ingresso viene riscaldato una volta. Si misurano poi 16 ripetizioni di
8 campioni per le coppie ordinarie e le tangenti, 16 ripetizioni per ciascun
caso quasi degenere e per il gruppo di snap, 3 per ciascuna misura grande.
`Calls` esclude il riscaldamento. La media aritmetica e il massimo sono
misurati con `std::chrono::steady_clock`, in microsecondi per chiamata: nessuna
media dei massimi e nessuna sottrazione stimata del costo dell'orologio.

La generazione degli ingressi e la stampa sono escluse dai tempi. Sono inclusi
chiamata pubblica, costruzione, lettura e distruzione del risultato, e il costo
della misura. Il checksum stampato rende osservabili coordinate e conteggi;
le verifiche del carico non sono nuovi riferimenti geometrici. Una diagnosi
inattesa o un offset non riuscito produce `INVALID` e uscita 1: quel tempo
non va presentato come costo di un'operazione riuscita. Il superamento di un
obiettivo di tempo non cambia il codice d'uscita.

| Riga | Carico misurato | Obiettivo dell'ADR |
|---|---|---|
| `intersect A/B` | Tutte le 15 coppie non ordinate tra segmento, arco di 3/4 di giro, cerchio, pezzo di polilinea curvo (bulge 1) e dritto; 8 traslazioni per coppia, senza prolungamenti | ADR 0010, punto 8: qualche microsecondo al più per una coppia non degenere |
| `near tangent ...` | Retta/cerchio e cerchio/cerchio ai due lati della tangenza, separati dalla soglia di 2^-40 | ADR 0010, punto 8: costo limitato del percorso esatto, senza una soglia numerica |
| `near coincident ...` | Cerchi con centri distanti 2^-40; rette quasi coincidenti che si attraversano | ADR 0010, punto 8: costo limitato vicino alle degenerazioni, senza una soglia numerica |
| `snap: 32 pairs` | 32 coppie di tipi misti nello stesso intervallo cronometrato | ADR 0010, punto 8: meno di 1 ms **per tutto il gruppo**, non per coppia |
| `intersectAlong N` | Un segmento contro 10, 100 o 1000 taglienti verticali, forniti nell'ordine inverso lungo l'ospite | ADR 0010, punto 8: misurare la crescita del costo; nessuna soglia specifica per `intersectAlong` |
| `tangentsFrom A` | Punto esterno contro arco, cerchio e pezzo curvo, con prolungamento; 8 punti esterni distinti | ADR 0011, punto 10: poche operazioni in virgola mobile e una radice nel caso comune; nessuna soglia numerica |
| `offsetPolyline N` | Scala ortogonale aperta di 10, 100 o 500 pezzi lunghi 20, distanza +1, unità millimetri; verifica `done`, `simple` e numero di origini | ADR 0012, punto 11: sotto il tempo di un comando per qualche centinaio di pezzi; nessuna soglia numerica |

I dati quasi degeneri esercitano i percorsi costosi, ma il banco non osserva
quale filtro interno sia stato usato: non include intestazioni private né
aggiunge strumentazione alla libreria. Le misure non provano da sole quale
algoritmo sia entrato in funzione.

La riga finale segnala automaticamente se il massimo del gruppo di snap
raggiunge o supera 1000 microsecondi. Gli altri obiettivi sono qualitativi:
la tabella li riporta senza inventare soglie. Se una misura supera l'obiettivo,
lo si scrive nella pull request; un miglioramento richiede una richiesta o un
nuovo pacchetto deciso dal responsabile, senza modificare qui la geometria.
