# Banco delle prestazioni del piano

`solid_space_banco_prestazioni` usa solo le API pubbliche di `geom2d` e
`robust`, senza librerie di benchmark. Si compila insieme ai test sui tre
sistemi, ma non è registrato in CTest e la CI non lo esegue. Le prestazioni
sono obiettivi da misurare, non requisiti che fanno fallire la CI.

## Compilazione ed esecuzione

Con gli strumenti di [AMBIENTE-SVILUPPO.md](../../docs/AMBIENTE-SVILUPPO.md),
dalla radice del repository, in Bash (anche su Windows):

```sh
cmake --preset release
cmake --build --preset release --target solid_space_banco_prestazioni
./build/release/tests/solid_space_banco_prestazioni
```

I comandi presuppongono `SS_BUILD_SUFFIX` non impostato. Gli agenti usano il
proprio suffisso (per esempio `SS_BUILD_SUFFIX=-codex` per entrambi i comandi
CMake) e avviano il banco dalla corrispondente cartella `build-codex/release`.

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
Le righe ellisse usano un campione e 16 ripetizioni; `tan`, `acos` e `hypot` usano 8 campioni e 16 ripetizioni.
`Calls` esclude il riscaldamento. La media aritmetica e il massimo sono
misurati con `std::chrono::steady_clock`, in microsecondi per chiamata: nessuna
media dei massimi e nessuna sottrazione stimata del costo dell'orologio.

La generazione degli ingressi e la stampa sono escluse dai tempi. Sono inclusi
chiamata pubblica, costruzione, lettura e distruzione del risultato, e il costo
della misura. Il checksum stampato rende osservabili coordinate e conteggi;
le verifiche del carico non sono nuovi riferimenti geometrici. Una diagnosi
inattesa, ingressi saltati o un esito del carico non riuscito producono
`INVALID` e uscita 1: quel tempo non va presentato come costo di un'operazione riuscita. Il superamento di un
obiettivo di tempo non cambia il codice d'uscita. Uscita 0 significa misure
completate con risultati validi, 1 segnala un risultato non valido, 2
un'eccezione inattesa durante l'esecuzione del banco (anche di allocazione):
in quest'ultimo caso la tabella può essere incompleta e va scartata.

| Riga | Carico misurato | Obiettivo dell'ADR |
|---|---|---|
| `intersect A/B` | Tutte le 15 coppie non ordinate tra segmento, arco di 3/4 di giro, cerchio, pezzo di polilinea curvo (bulge 1) e dritto; 8 traslazioni per coppia, senza prolungamenti | ADR 0010, punto 8: qualche microsecondo al più per una coppia non degenere |
| `near tangent ...` | Retta/cerchio e cerchio/cerchio ai due lati di tangenze diagonali, con δ = 2^-40 e scatole sovrapposte anche nei casi `outside` | ADR 0010, punto 8: costo limitato del percorso esatto, senza una soglia numerica |
| `near coincident ...` | Cerchi con centri distanti 2^-40; rette quasi coincidenti che si attraversano | ADR 0010, punto 8: costo limitato vicino alle degenerazioni, senza una soglia numerica |
| `snap: 32 pairs` | 32 coppie di tipi misti nello stesso intervallo cronometrato; caso peggiore, tutte le coppie si incontrano | ADR 0010, punto 8: meno di 1 ms **per tutto il gruppo**, non per coppia |
| `intersectAlong N` | Un segmento contro 10, 100 o 1000 taglienti verticali, forniti nell'ordine inverso lungo l'ospite | ADR 0010, punto 8: misurare la crescita del costo; nessuna soglia specifica per `intersectAlong` |
| `tangentsFrom A` | Punto esterno contro arco, cerchio e pezzo curvo, con prolungamento; 8 punti esterni distinti | ADR 0011, punto 10: poche operazioni in virgola mobile e una radice nel caso comune; nessuna soglia numerica |
| `offsetPolyline N` | Scala ortogonale aperta di 10, 100 o 500 pezzi lunghi 20, distanza +1, unità millimetri; verifica `done`, `simple` e numero di origini | ADR 0012, punto 11: sotto il tempo di un comando per qualche centinaio di pezzi; nessuna soglia numerica |
| `detectBoundary 2400 enclosed / notEnclosed` | Griglia 24 × 20 di asole raccordate: due segmenti, due archi semicircolari e un foro circolare per cella; punto dentro la prima asola e fuori a sinistra, sullo stesso disegno | ADR 0014, punto 9: poche decine di ms con qualche migliaio di curve |
| `clipToRegion 256 lines / 272 pieces` | 256 rette parallele intere su un quadrato suddiviso in 256 pezzi e 16 isole circolari; verifica assenza di ingressi saltati e tratti interni per ogni retta | ADR 0015, punto 9: pochi ms per tutto il retino |
| `nestContours N` | 100, 200 e 300 cerchi concentrici annidati; verifica profondità presenti, nessun conflitto e nessun ingresso saltato | Nessuna soglia: misura della crescita |
| `ellipse/A ordinary / near tangent` | Ellisse di semiassi 10 e 5 contro segmento, arco, cerchio e pezzi di polilinea curvi e dritti; le quasi tangenze spostano verso l'interno di 2^-40 la retta y = 5 o il cerchio di centro (0, 10), raggio 5, rispetto al centro dell'ellisse | ADR 0017, punto 9: qualche decina di µs per coppia non degenere; percorso quasi degenere riportato senza soglia |
| `ellipse/ellipse ordinary / near tangent` | Assi maggiori perpendicolari per la coppia ordinaria; assi paralleli e centri distanti 10 − 2^-40 per quella quasi tangente | ADR 0017, punto 9: qualche decina di µs per coppia non degenere; percorso quasi degenere riportato senza soglia |
| `ellipse nearest far / near evolute` | Punto relativo (20, 12), lontano dall'evoluta, e (7,5, 2^-40), vicino alla cuspide (7,5, 0) dell'evoluta dell'ellisse di semiassi 10 e 5 | ADR 0017, punto 9: qualche decina di µs per chiamata non degenere |
| `ellipse snap: 32 entities (nearest + intersect)` | 32 ellissi traslate dal seme, ciascuna con uno snap vicino e una prova d'intersezione con una retta nello stesso intervallo; comprende anche candidati che non incontrano la retta | ADR 0017, punto 9: meno di 1 ms per tutto il gruppo |
| `tan`, `acos`, `hypot` | 8 argomenti diadici dal seme, in [0, 0,5), per tangente e arcocoseno; gli stessi con seconda componente 0,75 per `hypot` | Nessuna soglia: misura per le note |

Nei casi quasi tangenti il primo cerchio ha centro (0, 0) e raggio 5.
La retta tangente in (3, 4) va da (11, −2) a (−5, 10): si traslano entrambi
gli estremi di (±3δ, ±4δ), ottenendo distanza 5 ± 5δ dal centro.
Il secondo cerchio, anch'esso di raggio 5, ha centro (6 ± 3δ, 8 ± 4δ),
a distanza 10 ± 5δ. Il segno più dà `outside`, il meno `inside`.
Le scatole si sovrappongono in tutti e quattro i casi, così il filtro delle
scatole non scarta quelli esterni prima dell'incontro esatto dei supporti.
Il banco non osserva quale filtro interno sia stato usato: non include
intestazioni private né aggiunge strumentazione alla libreria. Le misure non provano da sole quale
algoritmo sia entrato in funzione.

La riga finale segnala automaticamente se il massimo di uno dei due gruppi di snap
raggiunge o supera 1000 microsecondi. Gli altri obiettivi sono qualitativi:
la tabella li riporta senza inventare soglie. Se una misura supera l'obiettivo,
lo si scrive nella pull request; un miglioramento richiede una richiesta o un
nuovo pacchetto deciso dal responsabile, senza modificare qui la geometria.

Le coordinate del disegno, della regione e del centro dei contorni annidati
sono traslate dal generatore. Le dimensioni restano esatte e fisse. I punti
scelti per `detectBoundary` devono dare rispettivamente `enclosed` e
`notEnclosed`, senza curve saltate. Queste verifiche controllano che il banco
misuri il percorso voluto; i casi geometrici indipendenti restano nel corpus.
Le ellissi quasi tangenti e il punto vicino all'evoluta restano carichi
separati dai casi ordinari. Nessuna misura identifica da sola il filtro o
l'algoritmo interno usato.
