
# De Golffunctie Is Echt

Presentatienotities. Live simulator, beamer, ~15 mensen.
Publiek: wetenschappelijk nieuwsgierig, leest popsci, kent de
terminologie, nooit diep in de wiskunde gedoken.


## Opening

Toon de Schrödingervergelijking op het scherm. Pauze.

"Ik lees al twintig jaar over quantummechanica. En twintig jaar lang
staarde ik naar deze vergelijking zonder te weten wat ik ermee moest.
Het is een differentiaalvergelijking — het *doet* niets. Het zit daar
maar en beschrijft een relatie.

Maar als je er net even anders naar kijkt, zegt het iets heel simpels:
gegeven de toestand van het systeem op dit moment, zo verandert het in
het volgende ogenblik. En *dat* is iets wat ik in een loop kan zetten.
Neem de toestand op tijdstip nul, pas de regel toe, krijg tijdstip één.
Pas hem nog een keer toe, krijg tijdstip twee. Een miljoen keer per
seconde op een GPU. Wat je zo meteen gaat zien is niets meer dan dat —
één formule, steeds opnieuw toegepast. Alles wat er gebeurt, gebeurt
door die ene regel."


## 1-free-particle.lua — De Golffunctie

Start gepauzeerd. Alleen helix, geen oppervlak, geen kleur.

"Dit is een golffunctie. Geen artistieke impressie — dit zijn de
werkelijke data. 512 complexe getallen op een lijn, die één elektron
voorstellen."

Wijs de assen aan: horizontaal is positie in de ruimte (nanometers),
verticaal is de waarde. Leg nog niet uit wat de waarde betekent — laat
ze het gewoon zien.

Start de tijd. Laat het langzaam over het scherm driften.

"Het beweegt. Het spreidt uit. Dit is een vrij elektron op ongeveer
0,1 elektronvolt — de energieschaal van nano-elektronica."

Zet oppervlak/spaken aan om de discrete samples te tonen.

"Wat eruitziet als een vloeiende curve is eigenlijk gewoon een rij
getallen. Elke spaak is één complex getal — een punt op het grid."

Roteer naar 3D. De helix wordt zichtbaar als een spiraal.

"Dit is waarom het een golffunctie heet. Elke waarde heeft een reëel
en een imaginair deel. In 3D geplot is het een kurkentrekker. De
golflengte van die kurkentrekker *is* de impuls van het deeltje. Korte
golflengte betekent snel. Lang betekent langzaam."

Terug naar 2D. Zet omhullende aan, helix uit.

"Deze bult is de kansamplitude. Kwadrateer het en je krijgt de kans om
het deeltje op elke positie te vinden. Het elektron *is* niet op één
punt — het is uitgespreid, en deze vorm vertelt je waar je het
waarschijnlijk zult vinden."


## 2-standing-still.lua — Onzekerheid

"Wat gebeurt er als een deeltje perfect stilstaat? Nul impuls."

Laad en start. Het pakket spreidt zich meteen uit.

"Het kan niet stilstaan. Hoe preciezer je vastlegt waar het is, hoe
onzekerder de impuls wordt — en al die impulscomponenten vliegen uit
elkaar. Dit is geen beperking van de simulator. Dit is het
onzekerheidsprincipe, dat vanzelf ontstaat uit niets anders dan de
vergelijking."


## 3-reflection.lua — Tegen een Muur

Helix + omhullende.

"Nu staat er een muur in de weg. Kijk wat er gebeurt."

Laat het tegen de barrière botsen en reflecteren. Wijs op het
interferentiepatroon dat ontstaat als de gereflecteerde golf overlapt
met de inkomende golf.

"De omhullende laat iets moois zien: tijdens de botsing zijn er plekken
waar de kans naar nul daalt. Het deeltje kan daar letterlijk niet
gevonden worden. Dat is interferentie — de gereflecteerde golf die de
inkomende golf op specifieke punten uitdooft."

Zoom in op de rand van de barrière om de evanescente staart te tonen
die in de muur lekt.

"Ook al kaatst het terug, kijk: het lekt de muur in. De amplitude neemt
af in de barrière. Het komt er niet doorheen — de muur is te dik en te
hoog — maar het probeert het wel."


## 4-tunneling.lua — De Halve Spiegel

Helix + omhullende.

"Zelfde opstelling, maar nu is de barrière dunner en lager. Afgestemd
zodat precies de helft van de golf erdoorheen komt."

Start op normale snelheid. Kijk hoe het pakket splitst in twee bulten —
één gereflecteerd, één doorgelaten.

"Dit is quantum tunneling. Het deeltje raakte een muur en de helft lekte
erdoorheen. Maar hier is het punt: er is nog steeds maar één elektron.
We hebben niets gesplitst. De golffunctie heeft nu amplitude in twee
gebieden — en als je gaat zoeken naar het deeltje, vind je het aan de
ene kant of aan de andere. Nooit beide. Nooit ertussenin."

Zoom in op de barrière tijdens de splitsing om de golf erdoorheen te
zien lekken.

"Dit is een halve spiegel voor elektronen. Zo werken tunneldiodes, zo
brengen scanning tunneling microscopen individuele atomen in beeld. Geen
metafoor — dit is het daadwerkelijke mechanisme."


## 5-mach-zehnder.lua — De Interferometer

2D grid view bovenaan, helix in marginale modus eronder.

"Nu gaan we naar twee dimensies. Hetzelfde elektron, maar het kan in een
vlak bewegen. Er is een beam splitter — een dunne barrière, zelfde
tunnelprincipe — plus twee spiegels."

Start. Het pakket raakt de beam splitter, splitst in twee paden, kaatst
elk af op een spiegel, en ze komen weer samen bij de splitter.

"Kijk wat er gebeurt als de twee helften elkaar weer ontmoeten."

Ze recombineren. De ene uitgang krijgt alles, de andere niets.

"De twee helften waren nooit gescheiden. Ze waren altijd één golffunctie,
die fase opbouwde langs twee verschillende paden. Als ze samenkomen,
vallen de fases constructief samen in de ene richting en destructief in
de andere. Het deeltje *moet* naar rechts. Niet 50/50 — 100/0. De
golffunctie garandeert het."


## 6-which-path-1d.lua — Twee Deeltjes

Twee helix widgets, marginale modus, verticaal gestapeld — één voor
elk deeltje.

"Nu zijn er twee elektronen. Twee deeltjes betekent twee assen — de
horizontale as van de bovenste plot is de positie van deeltje A, de
onderste plot is de positie van deeltje B. Dit is niet langer de
fysieke ruimte — dit is configuratieruimte."

Start. A raakt de beam splitter en splitst. De doorgelaten helft bereikt
B en geeft het een duw via een korte-afstandsinteractie.

"A splitste in tweeën. De rechter helft botste tegen B en duwde het weg.
Maar de linker helft bereikte B niet — dus vanuit B's perspectief is er
niets gebeurd. B is nu in een superpositie: geduwd en niet geduwd,
tegelijkertijd."

Wijs naar B's marginaal — het toont twee pieken.

"B's toestand hangt nu af van wat A deed. Als A naar rechts ging, werd
B geduwd. Als A naar links ging, staat B nog steeds stil. Ze zijn
verstrengeld — je kunt de een niet beschrijven zonder de ander."

Schakel naar slice modus. Beweeg A's cursor.

"Als ik een positie kies voor A rechts van de barrière — staat B hier,
geduwd. Als ik een positie kies voor A links — is B niet verplaatst.
Dezelfde golffunctie, dezelfde simulatie. Ik kijk alleen naar
verschillende doorsnedes van hetzelfde object."


## 7-which-path-interferometer.lua — De Climax

2D grid, toont A's x-y vlak.

"Terug naar de interferometer. Dezelfde spiegels, dezelfde beam
splitter. Maar nu is er een tweede deeltje — een zware detector — dat
opzij staat, weg van beide armen."

Eerste run: B's cursor gecentreerd, weg van de actie.

"B staat uit de weg. A doet z'n ding — splitst, kaatst, recombineert.
Zelfde resultaat als eerder: alles gaat naar één uitgang."

Pauzeer. Herlaad met R. Verplaats B's cursor naar de linkerarm, tussen
de beam splitter en de spiegel.

"Nu staat B op de linkerarm. Start opnieuw."

Tweede run: de uitkomst is anders — A's kans is verdeeld over beide
uitgangen.

"De interferentie is weg. B had interactie met één arm van de
interferometer. Die interactie verstrengelde ze — B draagt nu informatie
over welk pad A nam. En die informatie, alleen al door te *bestaan*, is
genoeg om het interferentiepatroon te vernietigen."

Nu de clou. Pauzeer de simulatie.

"Maar dit is wat me echt raakt. Dit was één simulatie. Één golffunctie
die evolueerde onder één vergelijking. Kijk wat er gebeurt als ik B
verplaats."

Sleep B's cursor langzaam van de armpositie terug naar het midden en
verder. Het patroon op A's grid verandert continu — van gesplitste
uitvoer via schone recombinatie en terug.

"Elke positie van B geeft een ander antwoord voor A. Dit zijn geen
meerdere runs — het is één wiskundig object, en ik snij er doorheen.
Waar je kijkt bepaalt wat je ziet."

Beweeg A's cursor om het omgekeerde te tonen: voor elke uitkomst van A
is B in een andere toestand.

"En het werkt ook andersom. Kies waar A eindigde — nu verandert B's
toestand. Ze zijn één ding. Ze waren altijd één ding. De vergelijking
maakte ze tot één ding."


## Afsluiting

"Eén ding dat ik steeds opnieuw leer. Op school zeggen ze: je kunt niet
delen door nul. Dan komt calculus en blijkt dat het wel kan — je neemt
een limiet, en plotseling wordt alles over veranderingssnelheden simpel.
Ze zeggen: je kunt geen wortel trekken uit min één. Dan komen complexe
getallen, en polynomen en rotaties en signaalverwerking worden allemaal
*schoner* dan ze daarvoor waren.

De populair-wetenschappelijke versie van quantummechanica doet hetzelfde.
Het vertelt je: twee deeltjes hebben elk hun eigen golffunctie, en dan
gebeurt er iets magisch dat verstrengeling heet en dan delen ze er één.
Wanneer gebeurt dat? Is het onmiddellijk? Geleidelijk? Kun je een beetje
een golffunctie delen, zoals een beetje zwanger zijn?

Het antwoord is: er is tegen je gelogen. Er waren nooit twee
golffuncties. Er was er altijd één — over de gecombineerde
configuratieruimte van beide deeltjes. Als ze geen interactie hebben,
is die ene golffunctie toevallig factoriseerbaar, dus het *lijkt* op
twee aparte dingen. Op het moment dat ze interactie hebben, breekt de
factorisatie. Dat is alles wat verstrengeling is. Niet iets nieuws dat
verscheen. Een vereenvoudiging die ophield te werken.

Je zag dit net op het scherm. Het grid veranderde niet. De assen
veranderden niet. De golffunctie was altijd één object. Het hield
alleen op separabel te zijn.

Elke keer dat ik tegen zo'n muur aanliep — elke keer dat de echte
uitleg de versimpelde leugen verving — werden dingen simpeler, niet
moeilijker. De waarheid is eleganter dan de afkorting."

Pauze.

"Alles wat je net zag kwam voort uit één vergelijking, een miljoen keer
toegepast. Geen speciale gevallen, geen if-statements voor tunneling of
interferentie of verstrengeling. Het komt allemaal voort uit dezelfde
update-regel.

Ik heb honderden uren hieraan besteed en ik begrijp nog steeds niet wat
het betekent. Ik denk niet dat iemand dat doet. Maar het zien bewegen,
de golf zien splitsen en recombineren en verstrengelen — het hield op
abstract te zijn voor mij. Het kronkellijntje in de boeken is echt. Het
doet dingen. En de dingen die het doet zijn vreemder dan welk
populair-wetenschappelijk artikel ook kan overbrengen."
