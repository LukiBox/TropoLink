#include "ui/models/HelpContent.h"

#include <QVariantMap>

#include <array>

namespace {

struct Topic {
    const char* id;
    const char* titlePl;
    const char* titleEn;
};

constexpr std::array<Topic, 15> kTopics{{
    {"overview", "Pierwsze kroki", "Getting started"},
    {"map", "Mapa", "The map"},
    {"offline-maps", "Mapy offline — pobieranie", "Offline maps — downloading"},
    {"terrain", "Dane terenu (DEM)", "Terrain data (DEM)"},
    {"sites", "Stanowiska A i B", "Sites A and B"},
    {"radio", "Radio i antena", "Radio & antenna"},
    {"atmosphere", "Atmosfera", "Atmosphere"},
    {"models", "Modele propagacji", "Propagation models"},
    {"budget", "Bilans łącza", "Link budget"},
    {"availability", "Dostępność i dywersyfikacja", "Availability & diversity"},
    {"solver", "Solver projektowy", "Design solver"},
    {"profile", "Profil trasy", "Path profile"},
    {"report", "Raport PDF", "PDF report"},
    {"project", "Projekt i eksport", "Project & export"},
    {"about", "O programie", "About"},
}};

QString helpPl(const QString& t) {
    if (t == QLatin1String("overview")) {
        return QStringLiteral(
            u"<h3>Czym jest TropoLink</h3>"
            u"<p>TropoLink projektuje łącza radiowe z rozpraszaniem troposferycznym "
            u"(troposcatter) — łączność daleko poza horyzont bez satelity i bez "
            u"przemienników. Program działa w pełni offline.</p>"
            u"<h3>Przepływ pracy — 5 kroków</h3>"
            u"<ol>"
            u"<li><b>Ustaw stanowiska.</b> Przeciągnij pinezki A i B na mapie albo wpisz "
            u"współrzędne (DD, DMS, MGRS lub UTM) w panelu Stanowiska.</li>"
            u"<li><b>Sprawdź teren.</b> Pasek stanu pokazuje TEREN OK, gdy profil trasy "
            u"pokrywają dane DEM. Brakujące obszary pobierzesz (SRTM) lub zaimportujesz "
            u"(DTED/GeoTIFF) — patrz temat „Dane terenu”.</li>"
            u"<li><b>Wprowadź parametry radiowe.</b> Częstotliwość, moc, zyski anten, "
            u"modulacja i przepływność — panel Radio.</li>"
            u"<li><b>Czytaj wyniki.</b> Tabela modeli pokazuje kilka opublikowanych metod "
            u"obok siebie; rozrzut między nimi to rzeczywista niepewność projektu. Niżej "
            u"bilans łącza, margines i dostępność.</li>"
            u"<li><b>Generuj raport.</b> Przycisk „Raport PDF” tworzy formalny, dwujęzyczny "
            u"raport z odtwarzalnym skrótem SHA-256.</li>"
            u"</ol>"
            u"<p>Skróty: <code>Ctrl+O/S</code> projekt, <code>Ctrl+R</code> raport, "
            u"<code>Ctrl+L</code> język, <code>Ctrl+T</code> motyw, <code>F1</code> "
            u"samouczek.</p>");
    }
    if (t == QLatin1String("map")) {
        return QStringLiteral(
            u"<h3>Nawigacja</h3>"
            u"<p>Przeciąganie — przesuwanie mapy. Kółko myszy — zoom (podwójne kliknięcie "
            u"również przybliża). Prawy przycisk — menu: ustawienie stanowiska A/B, pomiar, "
            u"pobieranie terenu SRTM, import plików, wybór podkładu.</p>"
            u"<h3>Warstwy podkładu</h3>"
            u"<p>Lista „Podkład” na pasku mapy wybiera źródło:</p>"
            u"<ul>"
            u"<li><b>Teren (offline)</b> — mapa topograficzna rysowana z wczytanych danych "
            u"DEM: cieniowanie rzeźby, barwy hipsometryczne i poziomice z opisem "
            u"wysokości (co 5. poziomica pogrubiona). Działa zawsze, bez sieci.</li>"
            u"<li><b>OpenTopoMap / OSM (online)</b> — kafelki pobierane z internetu i "
            u"trwale buforowane na dysku; raz obejrzany obszar działa potem offline.</li>"
            u"<li><b>Pakiet MBTiles</b> — wczytany plik .mbtiles (własny lub pobrany w "
            u"programie). Ma pierwszeństwo przed pozostałymi warstwami.</li>"
            u"</ul>"
            u"<h3>Elementy na mapie</h3>"
            u"<p>Pomarańczowa linia — geodezyjna trasa łącza. Zielony romb — rzut wspólnej "
            u"objętości rozproszenia. Niebieskie wachlarze — sektory horyzontu radiowego "
            u"anten. Siatka MGRS — przełącznik „Siatka MGRS”; przy większym zoomie pojawia "
            u"się siatka 100 km. Pomiar — przycisk „Pomiar”, dwa kliknięcia dają odległość "
            u"i azymut. Pole szukania przyjmuje każdy format współrzędnych.</p>"
            u"<p>Odczyt w prawym dolnym rogu pokazuje pozycję kursora jednocześnie w DD, "
            u"DMS, MGRS i UTM.</p>");
    }
    if (t == QLatin1String("offline-maps")) {
        return QStringLiteral(
            u"<h3>Dwa rodzaje danych mapowych</h3>"
            u"<p><b>DEM (teren)</b> — wysokości terenu; z nich liczony jest profil, "
            u"horyzonty i strata trasy. <b>Podkład (basemap)</b> — obraz mapy pod spodem; "
            u"czysto poglądowy, nie wpływa na obliczenia. Ten temat dotyczy podkładów; "
            u"DEM opisuje temat „Dane terenu”.</p>"
            u"<h3>Pobieranie podkładu do pracy offline</h3>"
            u"<ol>"
            u"<li>Przy dostępie do internetu otwórz „Pobierz mapy…” na pasku mapy.</li>"
            u"<li>Wybierz obszar: bieżący widok albo korytarz wzdłuż trasy A–B.</li>"
            u"<li>Wybierz źródło (OpenTopoMap — rysunek topograficzny z poziomicami; OSM — "
            u"mapa ogólna) i zakres zoomów. Więcej zoomów = więcej szczegółów = więcej "
            u"danych; szacunek liczby kafelków i rozmiaru widać na bieżąco. Limit jednego "
            u"pobrania to 30 000 kafelków.</li>"
            u"<li>Start. Wynikowy plik .mbtiles zapisuje się w katalogu danych aplikacji i "
            u"zostaje od razu ustawiony jako podkład.</li>"
            u"</ol>"
            u"<p>Pobrany pakiet działa bezterminowo offline; można go też skopiować na inny "
            u"komputer (także z wersją Air-Gap) i wczytać przez „Wczytaj podkład "
            u"(MBTiles)…”.</p>"
            u"<h3>Zwykłe przeglądanie online też buforuje</h3>"
            u"<p>W trybie OpenTopoMap/OSM każdy obejrzany kafelek trafia do trwałej "
            u"pamięci podręcznej na dysku — obszar raz przejrzany będzie widoczny offline "
            u"nawet bez tworzenia pakietu.</p>"
            u"<h3>Pakiety z zewnątrz</h3>"
            u"<p>Program czyta standardowe pliki MBTiles (rastrowe). Pakiety takie "
            u"generują np. Mobile Atlas Creator (MOBAC) albo QGIS; wojskowe komórki "
            u"geograficzne mogą dostarczać własne. Wczytany pakiet może mieć dowolne "
            u"pokrycie i zoomy — poza jego zakresem mapa wraca do rysunku terenu.</p>"
            u"<h3>Wersja Air-Gap</h3>"
            u"<p>W odmianie Air-Gap nie ma żadnego kodu sieciowego: dostępne są rysunek "
            u"terenu z DEM oraz wczytane pakiety MBTiles.</p>");
    }
    if (t == QLatin1String("terrain")) {
        return QStringLiteral(
            u"<h3>Skąd program bierze wysokości</h3>"
            u"<p>Wszystkie obliczenia (profil, horyzonty, kąt rozproszenia, ITM) używają "
            u"magazynu DEM. Program czyta DTED 0/1/2, SRTM HGT i GeoTIFF. Z aplikacją "
            u"dostarczany jest DTED-0 dla zachodniej Polski — scenariusz referencyjny "
            u"działa od razu.</p>"
            u"<h3>Dodawanie danych</h3>"
            u"<ul>"
            u"<li><b>Import</b> — menu prawego przycisku na mapie → „Importuj pliki "
            u"terenu…”, albo przeciągnij pliki na okno programu.</li>"
            u"<li><b>Pobieranie SRTM</b> (wersja standardowa) — prawy przycisk → „Pobierz "
            u"SRTM dla widoku”: kafelki 1×1° (1 arcsec, ~30 m) dla widocznego obszaru, "
            u"limit 24 kafelków na raz.</li>"
            u"</ul>"
            u"<p>Panel „Teren” wylicza wczytane pliki z rozdzielczością, pochodzeniem "
            u"(import/pobrane) i zasięgiem; wpisy można usuwać.</p>"
            u"<h3>Braki danych</h3>"
            u"<p>Dziury (voidy) w DEM są interpolowane liniowo i <b>zawsze oznaczane</b> — "
            u"na profilu jako wyróżnione pasma, w pasku stanu jako ostrzeżenie. Program "
            u"nigdy nie wymyśla terenu bez oznaczenia. Gdy trasa w ogóle nie ma pokrycia, "
            u"modele terenozależne zgłaszają to zamiast liczyć fikcję.</p>");
    }
    if (t == QLatin1String("sites")) {
        return QStringLiteral(
            u"<h3>Współrzędne</h3>"
            u"<p>Pola przyjmują każdy z formatów: stopnie dziesiętne "
            u"(<code>51.506 15.331</code>), DMS (<code>51°30'22\"N 15°19'53\"E</code>), "
            u"MGRS (<code>33UXS 60443 06212</code>) i UTM "
            u"(<code>33U 460443 5706212</code>). Zatwierdzenie Enterem parsuje wpis; "
            u"nierozpoznany format zgłaszany jest w pasku stanu. Pinezki na mapie można "
            u"też po prostu przeciągać — wyniki przeliczają się w locie.</p>"
            u"<h3>Wysokość anteny (AGL)</h3>"
            u"<p>Wysokość środka apertury nad gruntem, nie nad poziomem morza. Podnosi "
            u"antenę ponad przeszkody lokalnego horyzontu — kilka metrów potrafi wyraźnie "
            u"zmienić kąt startowy θt i całą stratę trasy.</p>"
            u"<h3>Presety sprzętu</h3>"
            u"<p>Listy anten i radiostacji pochodzą z edytowalnego pliku "
            u"<code>data/equipment.json</code>; wybór presetu wypełnia zysk, średnicę i "
            u"moc jednym kliknięciem. Wartości można potem dowolnie nadpisać.</p>");
    }
    if (t == QLatin1String("radio")) {
        return QStringLiteral(
            u"<h3>Parametry</h3>"
            u"<ul>"
            u"<li><b>Częstotliwość</b> — pasmo pracy w GHz. Modele mają zakresy ważności "
            u"(P.617: 0,2–5 GHz); poza nimi wiersz modelu mówi „poza zakresem”.</li>"
            u"<li><b>Moc nadajnika</b> — pole rozumie jednostki: <code>500 W</code>, "
            u"<code>57 dBm</code>, <code>27 dBW</code>; sama liczba = dBm.</li>"
            u"<li><b>Zysk anteny A/B</b> — dBi. Uwaga: suma zysków zwiększa też stratę "
            u"sprzężenia apertura–ośrodek (patrz „Modele”).</li>"
            u"<li><b>Straty falowodu</b> — tłumienie toru antenowego każdej strony, dB.</li>"
            u"<li><b>Współczynnik szumów</b> — NF odbiornika, dB; wyznacza próg "
            u"szumów.</li>"
            u"<li><b>Modulacja i przepływność</b> — para wyznacza szerokość pasma i "
            u"wymagany Eb/N0 (wartości teoretyczne przy BER 1e-6, edytowalne w "
            u"<code>data/modulations.json</code>).</li>"
            u"<li><b>Średnica anteny</b> — używana do obliczeń rozstawień "
            u"dywersyfikacyjnych (P.617-5 §7).</li>"
            u"</ul>"
            u"<h3>Auto-dobór radia</h3>"
            u"<p>Przycisk <b>„Auto-dobór radia z geometrii”</b> dobiera całą "
            u"konfigurację (pasmo, średnicę anteny, zysk, moc, modulację) tak, by "
            u"spełnić docelową dostępność przy bieżącej przepływności — z zapasem "
            u"projektowym 3 dB. Program przeszukuje realne pasma i średnice anten, "
            u"licząc każdą kombinację modelem P.617-5, i wybiera tę wymagającą "
            u"najmniejszej mocy nadajnika (przy remisie: mniejsza antena, niższe "
            u"pasmo). Uwzględnia stratę sprzężenia apertura–ośrodek, więc nie wpada w "
            u"pułapkę „większa antena zawsze lepsza”. Po kliknięciu pojawia się krótkie "
            u"podsumowanie zmian z uzasadnieniem; wszystkie pola pozostają edytowalne.</p>");
    }
    if (t == QLatin1String("atmosphere")) {
        return QStringLiteral(
            u"<h3>Refrakcja</h3>"
            u"<p><b>N0</b> (refrakcyjność na poziomie morza) i <b>ΔN</b> (gradient) "
            u"odczytywane są automatycznie z cyfrowych map ITU-R (P.452, integralna część "
            u"P.617-5) w punkcie środkowym trasy — stąd wartości zmieniają się, gdy "
            u"przesuwasz stanowiska.</p>"
            u"<p><b>Współczynnik k</b> (promień zastępczy Ziemi, k = 157/(157−ΔN)) domyślnie "
            u"wynika z ΔN; tryb ręczny pozwala badać warunki anomalne (subrefrakcja "
            u"k&lt;4/3, superrefrakcja k&gt;4/3). Przełącznik „auto” przywraca wartość "
            u"mapową.</p>"
            u"<h3>Klimat</h3>"
            u"<p>Strefa klimatyczna NBS TN101/ITM steruje zmiennością długoterminową "
            u"modelu TN101 i ITM. Dla Polski właściwy jest „Continental Temperate”.</p>");
    }
    if (t == QLatin1String("models")) {
        return QStringLiteral(
            u"<h3>Dlaczego kilka modeli</h3>"
            u"<p>Modele troposcatter potrafią różnić się o kilka dB. TropoLink liczy "
            u"wszystkie równolegle i pokazuje <b>rozrzut jako niepewność projektu</b> — "
            u"zamiast udawać jedną „prawdziwą” liczbę.</p>"
            u"<ul>"
            u"<li><b>FSPL</b> (P.525) — strata wolnej przestrzeni; sam odnośnik, nie "
            u"prognoza troposcatter.</li>"
            u"<li><b>ITU-R P.617-5</b> — model podstawowy; równania (1)–(7) wprost z "
            u"Zalecenia, N0/ΔN z map cyfrowych. Ważność: 100–1000 km, 0,2–5 GHz.</li>"
            u"<li><b>NBS TN101</b> — klasyczna metoda (Rice i in., 1967) w postaci "
            u"skomputeryzowanej przez ITS; kotwica historyczna.</li>"
            u"<li><b>ITM (Longley-Rice)</b> — oficjalny kod NTIA na rzeczywistym profilu; "
            u"niezależna kontrola krzyżowa. Wiersz podaje też wybrany przez ITM mechanizm "
            u"(LOS/dyfrakcja/rozproszenie).</li>"
            u"</ul>"
            u"<h3>Strata sprzężenia</h3>"
            u"<p>Lc = 0,07·exp[0,055(Gt+Gr)] (P.617-5 eq. 3) — kara za bardzo zyskowne "
            u"anteny oświetlające małą wspólną objętość; przy 2×39 dBi to ok. 5 dB. "
            u"Pokazywana jako osobna pozycja, bo to klasyczna pułapka.</p>"
            u"<h3>Model podstawowy</h3>"
            u"<p>Lista „Model podstawowy” wybiera, który model zasila bilans, margines i "
            u"dostępność; pozostałe pozostają widoczne dla porównania. Każda wartość ma w "
            u"dymku swój wzór i źródło.</p>");
    }
    if (t == QLatin1String("budget")) {
        return QStringLiteral(
            u"<h3>Kaskada bilansu</h3>"
            u"<p>Wykres wodospadowy prowadzi od mocy nadajnika do marginesu:</p>"
            u"<p><code>EIRP = Ptx − Ltor + Gtx</code> → minus strata trasy (mediana "
            u"modelu podstawowego, ze stratą sprzężenia) → <code>RSL</code> (poziom "
            u"odbierany) → porównanie z progiem szumów "
            u"<code>−174 dBm/Hz + 10·log B + NF</code> → <code>SNR</code> → minus wymagany "
            u"SNR modulacji → <b>margines zaniku</b>.</p>"
            u"<p>Suma pozycji zgadza się co do dB z definicji — kaskada jest budowana z "
            u"tych samych liczb, które widać w tabelach.</p>"
            u"<p>Margines to zapas ponad próg dla <i>mediany</i> propagacji; o tym, jak "
            u"często zanik go przekroczy, mówi dostępność (osobny temat).</p>");
    }
    if (t == QLatin1String("availability")) {
        return QStringLiteral(
            u"<h3>Model zaniku</h3>"
            u"<p>Zanik długoterminowy (rozkład median godzinowych, z modelu podstawowego) "
            u"składany jest z zanikiem szybkim Rayleigha wewnątrz godziny. Dostępność "
            u"roczna i najgorszego miesiąca (konwersja wg ITU-R P.841-5) liczona jest z "
            u"marginesu przez odwrócenie tej krzywej.</p>"
            u"<h3>Dywersyfikacja</h3>"
            u"<p>Tryby: brak / przestrzenna / częstotliwościowa / kątowa / poczwórna "
            u"(quad). Odbiór selektywny niezależnych gałęzi redukuje zanik szybki; "
            u"panel podaje wymagane rozstawy z P.617-5 §7: Δh i Δv dla przestrzennej, "
            u"Δf dla częstotliwościowej, Δθ dla kątowej — przy bieżącej geometrii i "
            u"średnicy anten. Rozstaw mniejszy od zalecanego = gałęzie skorelowane i "
            u"mniejszy zysk niż obliczony.</p>"
            u"<h3>Cel projektowy</h3>"
            u"<p>Pole „Dostępność docelowa” z przełącznikiem rok/najgorszy miesiąc zasila "
            u"solver oraz kolor oceny (zielony/żółty/czerwony) w wynikach i raporcie.</p>");
    }
    if (t == QLatin1String("solver")) {
        return QStringLiteral(
            u"<h3>Zadanie odwrotne</h3>"
            u"<p>Zamiast pytać „co osiągnę tym sprzętem”, solver odpowiada „czego "
            u"potrzebuję, by osiągnąć cel”. Przy bieżącej geometrii, modelu podstawowym i "
            u"dostępności docelowej wyznacza:</p>"
            u"<ul>"
            u"<li><b>Moc</b> — minimalna moc nadajnika (dBm i W);</li>"
            u"<li><b>Zysk</b> — minimalny zysk anten (przy założeniu tej samej anteny po "
            u"obu stronach; uwzględnia rosnącą stratę sprzężenia!);</li>"
            u"<li><b>Przepływność</b> — maksymalna przepływność bieżącej modulacji "
            u"mieszcząca się w marginesie.</li>"
            u"</ul>"
            u"<p>Wynik „niewykonalne” oznacza, że cel jest poza zasięgiem rozsądnych "
            u"wartości — zmień geometrię (wyższe anteny, krótsza trasa), dywersyfikację "
            u"albo cel dostępności.</p>");
    }
    if (t == QLatin1String("profile")) {
        return QStringLiteral(
            u"<h3>Co widać</h3>"
            u"<p>Przekrój terenu wzdłuż geodezyjnej A–B w przestrzeni „ziemi zastępczej” "
            u"(do wysokości dodana strzałka ugięcia d²/2ka) — dzięki temu promienie "
            u"radiowe są liniami prostymi. Niebieskie linie — promienie horyzontowe obu "
            u"anten; ich przecięcie wyznacza wspólną objętość rozproszenia (soczewka). "
            u"Przerywana — promień bezpośredni z pierwszą strefą Fresnela; pasmo "
            u"kreskowane — odcinki interpolowanych voidów DEM.</p>"
            u"<p>Najechanie kursorem pokazuje odległość i wysokość; znacznik "
            u"synchronizuje się z mapą. Adnotacja θ podaje kąt rozproszenia w mrad; "
            u"soczewka pokazuje też wysokość podstawy i szczytu wspólnej objętości.</p>"
            u"<p>Im niżej soczewka i im mniejszy θ, tym mniejsza strata — dlatego "
            u"podnoszenie anten i czyste horyzonty są tak cenne.</p>");
    }
    if (t == QLatin1String("report")) {
        return QStringLiteral(
            u"<h3>Zawartość</h3>"
            u"<p>Formalny raport projektowy: parametry wejściowe, geometria, tabela "
            u"modeli z rozrzutem, bilans, dostępność z dywersyfikacją, mapa i profil, "
            u"źródła danych terenu, wersje modeli — z oceną wykonalności "
            u"(zielona/żółta/czerwona) na stronie tytułowej.</p>"
            u"<h3>Odtwarzalność</h3>"
            u"<p>Stopka zawiera skrót SHA-256 treści raportu. Te same dane wejściowe i "
            u"wersja programu dają <b>identyczny</b> skrót — raport można audytować. "
            u"Zmiana języka zmienia treść, więc i skrót.</p>"
            u"<p>Tryb bezokienkowy: <code>TropoLink.exe --report plik.pdf --lang pl</code>.</p>"
            u"<h3>Komentarz AI (opcjonalny)</h3>"
            u"<p>Jeśli działa lokalna Ollama (wyłącznie 127.0.0.1), raport może zawierać "
            u"akapit komentarza. Wersja Air-Gap nie zawiera tego kodu w ogóle.</p>");
    }
    if (t == QLatin1String("project")) {
        return QStringLiteral(
            u"<h3>Pliki .tlk</h3>"
            u"<p>Projekt (stanowiska, radio, atmosfera, cele, migawki wyników z wersjami "
            u"modeli) zapisuje się w wersjonowanym pliku .tlk (JSON w archiwum zip). "
            u"<code>Ctrl+S</code>/<code>Ctrl+O</code>. „Referencyjny” przywraca scenariusz "
            u"podręcznikowy 103 km.</p>"
            u"<h3>Eksport</h3>"
            u"<ul>"
            u"<li><b>KML</b> — trasa, stanowiska i wspólna objętość do Google Earth;</li>"
            u"<li><b>CSV profilu</b> — kolumny odległość/wysokość/void;</li>"
            u"<li><b>CSV bilansu</b> — pełna kaskada i tabela modeli.</li>"
            u"</ul>");
    }
    if (t == QLatin1String("about")) {
        return QStringLiteral(
            u"<h3>TropoLink</h3>"
            u"<p>Przyrząd do projektowania łączy troposferycznych. Wersja programu "
            u"widoczna jest na pasku tytułu.</p>"
            u"<h3>Autor</h3>"
            u"<p><b>Program stworzył LukiBox.</b><br>"
            u"<a href=\"https://github.com/LukiBox\">https://github.com/LukiBox</a></p>"
            u"<h3>Licencja</h3>"
            u"<p>TropoLink udostępniany jest na licencji <b>Apache License 2.0</b> "
            u"(plik <code>LICENSE</code>). Komponenty zewnętrzne zachowują własne "
            u"licencje — pełny wykaz w plikach <code>NOTICE</code> i "
            u"<code>docs/third_party.md</code>.</p>"
            u"<p>W szczególności <b>Qt 6 wykorzystywane jest na licencji LGPL v3 i "
            u"linkowane dynamicznie</b>: biblioteki Qt dostarczane są jako osobne pliki "
            u"DLL obok programu i mogą zostać podmienione, czego wymaga LGPL v3 §4.</p>"
            u"<p>Model ITM (Longley-Rice) pochodzi z oficjalnej, należącej do domeny "
            u"publicznej implementacji NTIA i jest dołączony bez zmian.</p>"
            u"<h3>Prywatność</h3>"
            u"<p>Brak telemetrii. Jedyny ruch sieciowy, jaki może wystąpić, to "
            u"świadomie uruchomione przez operatora pobieranie map lub danych SRTM. "
            u"Wersja Air-Gap nie zawiera w ogóle kodu sieciowego.</p>");
    }
    return {};
}

QString helpEn(const QString& t) {
    if (t == QLatin1String("overview")) {
        return QStringLiteral(
            u"<h3>What TropoLink is</h3>"
            u"<p>TropoLink designs troposcatter radio links — far-beyond-horizon "
            u"communication with no satellite and no relays. The application works fully "
            u"offline.</p>"
            u"<h3>Workflow — 5 steps</h3>"
            u"<ol>"
            u"<li><b>Place the sites.</b> Drag pins A and B on the map or type coordinates "
            u"(DD, DMS, MGRS or UTM) in the Sites panel.</li>"
            u"<li><b>Check terrain.</b> The status bar shows TERRAIN OK when DEM data "
            u"covers the path. Download missing areas (SRTM) or import files (DTED/GeoTIFF) "
            u"— see the “Terrain data” topic.</li>"
            u"<li><b>Enter the radio parameters.</b> Frequency, power, antenna gains, "
            u"modulation and data rate — the Radio panel.</li>"
            u"<li><b>Read the results.</b> The model table shows several published methods "
            u"side by side; their spread is the real design uncertainty. Below: link "
            u"budget, margin, availability.</li>"
            u"<li><b>Generate the report.</b> “Report PDF” writes a formal bilingual "
            u"report with a reproducible SHA-256 hash.</li>"
            u"</ol>"
            u"<p>Shortcuts: <code>Ctrl+O/S</code> project, <code>Ctrl+R</code> report, "
            u"<code>Ctrl+L</code> language, <code>Ctrl+T</code> theme, <code>F1</code> "
            u"tour.</p>");
    }
    if (t == QLatin1String("map")) {
        return QStringLiteral(
            u"<h3>Navigation</h3>"
            u"<p>Drag to pan. Mouse wheel zooms (double-click zooms in too). Right-click "
            u"opens the context menu: set Site A/B, measure, download SRTM terrain, import "
            u"files, choose a basemap.</p>"
            u"<h3>Basemap layers</h3>"
            u"<p>The “Basemap” selector on the map bar chooses the source:</p>"
            u"<ul>"
            u"<li><b>Terrain (offline)</b> — a topographic rendering drawn from the loaded "
            u"DEM data: relief shading, hypsometric tints and elevation contours with "
            u"labels (every 5th contour emphasized). Always available, no network.</li>"
            u"<li><b>OpenTopoMap / OSM (online)</b> — tiles fetched from the internet and "
            u"cached permanently on disk; once an area has been viewed it works offline "
            u"afterwards.</li>"
            u"<li><b>MBTiles pack</b> — a loaded .mbtiles file (your own or downloaded "
            u"in-app). Takes priority over the other layers.</li>"
            u"</ul>"
            u"<h3>Map elements</h3>"
            u"<p>Orange line — the geodesic link path. Green diamond — the common scatter "
            u"volume's ground projection. Blue fans — each antenna's radio-horizon sector. "
            u"MGRS grid — the “MGRS grid” toggle; zoomed in, the 100 km square grid "
            u"appears. Measure — the “Measure” button, two clicks give distance and "
            u"azimuth. The search box accepts any coordinate format.</p>"
            u"<p>The bottom-right readout shows the cursor position in DD, DMS, MGRS and "
            u"UTM simultaneously.</p>");
    }
    if (t == QLatin1String("offline-maps")) {
        return QStringLiteral(
            u"<h3>Two kinds of map data</h3>"
            u"<p><b>DEM (terrain)</b> — ground elevations; profiles, horizons and path "
            u"loss are computed from these. <b>Basemap</b> — the map picture underneath; "
            u"purely visual, never used in computation. This topic covers basemaps; DEM "
            u"has its own “Terrain data” topic.</p>"
            u"<h3>Downloading a basemap for offline use</h3>"
            u"<ol>"
            u"<li>While online, open “Download maps…” on the map bar.</li>"
            u"<li>Pick the area: the current view or a corridor along the A–B path.</li>"
            u"<li>Pick the source (OpenTopoMap — topographic drawing with contours; OSM — "
            u"general-purpose map) and the zoom range. More zoom levels = more detail = "
            u"more data; the live estimate shows tile count and size. One download is "
            u"limited to 30,000 tiles.</li>"
            u"<li>Start. The resulting .mbtiles file lands in the application data folder "
            u"and becomes the active basemap immediately.</li>"
            u"</ol>"
            u"<p>A downloaded pack works offline indefinitely and can be copied to another "
            u"machine (including Air-Gap installations) and loaded via “Load basemap "
            u"(MBTiles)…”.</p>"
            u"<h3>Plain online browsing caches too</h3>"
            u"<p>In OpenTopoMap/OSM mode every viewed tile goes into a persistent disk "
            u"cache — an area browsed once stays visible offline even without building a "
            u"pack.</p>"
            u"<h3>Third-party packs</h3>"
            u"<p>TropoLink reads standard raster MBTiles. Such packs are produced by e.g. "
            u"Mobile Atlas Creator (MOBAC) or QGIS; military geo cells may issue their "
            u"own. A loaded pack may have any coverage and zooms — outside its range the "
            u"map falls back to the terrain rendering.</p>"
            u"<h3>Air-Gap flavor</h3>"
            u"<p>The Air-Gap build contains no network code at all: the DEM terrain "
            u"rendering and loaded MBTiles packs are available.</p>");
    }
    if (t == QLatin1String("terrain")) {
        return QStringLiteral(
            u"<h3>Where elevations come from</h3>"
            u"<p>All computation (profile, horizons, scatter angle, ITM) uses the DEM "
            u"store. TropoLink reads DTED 0/1/2, SRTM HGT and GeoTIFF. DTED-0 for western "
            u"Poland ships with the application, so the reference scenario works out of "
            u"the box.</p>"
            u"<h3>Adding data</h3>"
            u"<ul>"
            u"<li><b>Import</b> — right-click the map → “Import terrain files…”, or drop "
            u"files onto the window.</li>"
            u"<li><b>SRTM download</b> (standard flavor) — right-click → “Download SRTM "
            u"for this view”: 1×1° tiles (1 arcsec, ~30 m) for the visible area, at most "
            u"24 tiles per request.</li>"
            u"</ul>"
            u"<p>The “Terrain” panel lists loaded files with resolution, provenance "
            u"(imported/downloaded) and coverage; entries can be removed.</p>"
            u"<h3>Missing data</h3>"
            u"<p>DEM voids are interpolated linearly and <b>always flagged</b> — as "
            u"highlighted spans on the profile and a status-bar warning. TropoLink never "
            u"invents terrain silently. When a path has no coverage at all, "
            u"terrain-dependent models say so instead of computing fiction.</p>");
    }
    if (t == QLatin1String("sites")) {
        return QStringLiteral(
            u"<h3>Coordinates</h3>"
            u"<p>The fields accept any format: decimal degrees "
            u"(<code>51.506 15.331</code>), DMS (<code>51°30'22\"N 15°19'53\"E</code>), "
            u"MGRS (<code>33UXS 60443 06212</code>) and UTM "
            u"(<code>33U 460443 5706212</code>). Enter parses the text; an unrecognized "
            u"format is reported in the status bar. Map pins can simply be dragged — "
            u"results recompute live.</p>"
            u"<h3>Antenna height (AGL)</h3>"
            u"<p>Height of the aperture centre above ground, not above sea level. It "
            u"lifts the antenna over local horizon obstructions — a few metres can change "
            u"the takeoff angle θt and the whole path loss noticeably.</p>"
            u"<h3>Equipment presets</h3>"
            u"<p>The antenna and radio lists come from the editable "
            u"<code>data/equipment.json</code>; picking a preset fills gain, diameter and "
            u"power in one click. Values can be overridden freely afterwards.</p>");
    }
    if (t == QLatin1String("radio")) {
        return QStringLiteral(
            u"<h3>Parameters</h3>"
            u"<ul>"
            u"<li><b>Frequency</b> — operating band in GHz. Models have validity ranges "
            u"(P.617: 0.2–5 GHz); outside them the model row says “out of range”.</li>"
            u"<li><b>Transmit power</b> — the field understands units: <code>500 W</code>, "
            u"<code>57 dBm</code>, <code>27 dBW</code>; a bare number means dBm.</li>"
            u"<li><b>Antenna gain A/B</b> — dBi. Note the gain sum also increases the "
            u"aperture-to-medium coupling loss (see “Models”).</li>"
            u"<li><b>Line losses</b> — feeder losses per side, dB.</li>"
            u"<li><b>Noise figure</b> — receiver NF, dB; sets the noise floor.</li>"
            u"<li><b>Modulation & data rate</b> — the pair determines bandwidth and "
            u"required Eb/N0 (theoretical values at BER 1e-6, editable in "
            u"<code>data/modulations.json</code>).</li>"
            u"<li><b>Antenna diameter</b> — used for the diversity separation formulas "
            u"(P.617-5 §7).</li>"
            u"</ul>"
            u"<h3>Auto-design</h3>"
            u"<p>The <b>“Auto-design radio from geometry”</b> button solves the whole "
            u"configuration (band, antenna diameter, gain, power, modulation) to meet "
            u"the target availability at the current data rate, with 3 dB design "
            u"headroom. It sweeps real bands and dish sizes, scores every combination "
            u"with the P.617-5 model, and picks the one needing the least transmit "
            u"power (ties broken toward the smaller antenna and lower band). It accounts "
            u"for the aperture-to-medium coupling loss, so it does not fall for the "
            u"“bigger dish is always better” trap. A short summary of the changes and "
            u"the reasoning appears after the click; every field stays editable.</p>");
    }
    if (t == QLatin1String("atmosphere")) {
        return QStringLiteral(
            u"<h3>Refraction</h3>"
            u"<p><b>N0</b> (sea-level refractivity) and <b>ΔN</b> (lapse) are read "
            u"automatically from the ITU-R digital maps (P.452, integral to P.617-5) at "
            u"the path midpoint — the values change as you move the sites.</p>"
            u"<p>The <b>k-factor</b> (effective Earth radius, k = 157/(157−ΔN)) follows ΔN "
            u"by default; manual mode lets you study anomalous conditions (subrefraction "
            u"k&lt;4/3, superrefraction k&gt;4/3). The “auto” switch restores the map "
            u"value.</p>"
            u"<h3>Climate</h3>"
            u"<p>The NBS TN101/ITM climate zone drives long-term variability in the TN101 "
            u"and ITM models. For Poland, “Continental Temperate” is correct.</p>");
    }
    if (t == QLatin1String("models")) {
        return QStringLiteral(
            u"<h3>Why several models</h3>"
            u"<p>Troposcatter models can disagree by several dB. TropoLink runs them all "
            u"and shows the <b>spread as the design uncertainty</b> — instead of "
            u"pretending there is one “true” number.</p>"
            u"<ul>"
            u"<li><b>FSPL</b> (P.525) — free-space loss; a reference line, not a "
            u"troposcatter prediction.</li>"
            u"<li><b>ITU-R P.617-5</b> — the primary model; equations (1)–(7) straight "
            u"from the Recommendation, N0/ΔN from the digital maps. Validity: 100–1000 km, "
            u"0.2–5 GHz.</li>"
            u"<li><b>NBS TN101</b> — the classic method (Rice et al., 1967) in its ITS "
            u"computerized form; the historical anchor.</li>"
            u"<li><b>ITM (Longley-Rice)</b> — the official NTIA code over the real "
            u"profile; an independent cross-check. Its row also reports the mechanism ITM "
            u"selected (LOS/diffraction/scatter).</li>"
            u"</ul>"
            u"<h3>Coupling loss</h3>"
            u"<p>Lc = 0.07·exp[0.055(Gt+Gr)] (P.617-5 eq. 3) — the penalty for very "
            u"high-gain antennas illuminating a small common volume; at 2×39 dBi it is "
            u"about 5 dB. Shown as its own line because it is the classic trap.</p>"
            u"<h3>Primary model</h3>"
            u"<p>The “Primary model” selector chooses which model feeds the budget, "
            u"margin and availability; the others stay visible for comparison. Every "
            u"value carries its formula and source in a tooltip.</p>");
    }
    if (t == QLatin1String("budget")) {
        return QStringLiteral(
            u"<h3>The waterfall</h3>"
            u"<p>The chart walks from transmitter power to margin:</p>"
            u"<p><code>EIRP = Ptx − Lline + Gtx</code> → minus path loss (the primary "
            u"model's median, coupling included) → <code>RSL</code> (received signal "
            u"level) → against the noise floor "
            u"<code>−174 dBm/Hz + 10·log B + NF</code> → <code>SNR</code> → minus the "
            u"modulation's required SNR → the <b>fade margin</b>.</p>"
            u"<p>The lines sum exactly by construction — the waterfall is built from the "
            u"same numbers the tables show.</p>"
            u"<p>The margin is headroom above threshold for the propagation "
            u"<i>median</i>; how often fading eats it is what availability answers "
            u"(separate topic).</p>");
    }
    if (t == QLatin1String("availability")) {
        return QStringLiteral(
            u"<h3>Fading model</h3>"
            u"<p>Long-term fading (the distribution of hourly medians, from the primary "
            u"model) is combined with Rayleigh fast fading within the hour. Annual and "
            u"worst-month availability (ITU-R P.841-5 conversion) comes from inverting "
            u"that curve at the fade margin.</p>"
            u"<h3>Diversity</h3>"
            u"<p>Modes: none / space / frequency / angle / quad. Selection combining of "
            u"independent branches reduces fast fading; the panel gives the required "
            u"separations from P.617-5 §7: Δh and Δv for space, Δf for frequency, Δθ for "
            u"angle — at the current geometry and antenna diameter. Smaller separation "
            u"than recommended = correlated branches and less gain than computed.</p>"
            u"<h3>Design target</h3>"
            u"<p>The “Target availability” field with the annual/worst-month switch feeds "
            u"the solver and the go/marginal/no-go colour in the results and report.</p>");
    }
    if (t == QLatin1String("solver")) {
        return QStringLiteral(
            u"<h3>The inverse problem</h3>"
            u"<p>Instead of asking “what do I get with this equipment”, the solver "
            u"answers “what do I need to hit the target”. At the current geometry, "
            u"primary model and target availability it finds:</p>"
            u"<ul>"
            u"<li><b>Power</b> — minimum transmit power (dBm and W);</li>"
            u"<li><b>Gain</b> — minimum antenna gain (same antenna both ends assumed; the "
            u"growing coupling loss is accounted for!);</li>"
            u"<li><b>Data rate</b> — the maximum rate of the current modulation that "
            u"still fits the margin.</li>"
            u"</ul>"
            u"<p>“Infeasible” means the target is beyond reasonable values — change the "
            u"geometry (taller antennas, shorter path), the diversity, or the "
            u"availability target.</p>");
    }
    if (t == QLatin1String("profile")) {
        return QStringLiteral(
            u"<h3>What is shown</h3>"
            u"<p>The terrain cross-section along the A–B geodesic in effective-earth "
            u"space (the bulge d²/2ka is added to heights) — which makes radio rays "
            u"straight lines. Blue lines — both antennas' horizon rays; their crossing "
            u"defines the common scatter volume (the lens). Dashed — the direct ray with "
            u"its first Fresnel zone; hatched spans — interpolated DEM voids.</p>"
            u"<p>Hovering shows distance and elevation; the marker syncs with the map. "
            u"The θ annotation gives the scatter angle in mrad; the lens also shows the "
            u"common volume's base and top heights.</p>"
            u"<p>The lower the lens and the smaller θ, the lower the loss — which is why "
            u"raising antennas and clean horizons matter so much.</p>");
    }
    if (t == QLatin1String("report")) {
        return QStringLiteral(
            u"<h3>Contents</h3>"
            u"<p>A formal design report: inputs, geometry, the model table with spread, "
            u"budget, availability with diversity, map and profile, terrain data sources, "
            u"model versions — with the feasibility verdict (green/yellow/red) on the "
            u"title page.</p>"
            u"<h3>Reproducibility</h3>"
            u"<p>The footer carries a SHA-256 hash of the report content. The same inputs "
            u"and program version give an <b>identical</b> hash — the report is "
            u"auditable. Changing the language changes the content, hence the hash.</p>"
            u"<p>Headless mode: <code>TropoLink.exe --report file.pdf --lang en</code>.</p>"
            u"<h3>AI commentary (optional)</h3>"
            u"<p>If a local Ollama is running (127.0.0.1 only), the report can include a "
            u"commentary paragraph. The Air-Gap flavor does not contain this code at "
            u"all.</p>");
    }
    if (t == QLatin1String("project")) {
        return QStringLiteral(
            u"<h3>.tlk files</h3>"
            u"<p>The project (sites, radio, atmosphere, targets, result snapshots with "
            u"model versions) is stored in a versioned .tlk file (JSON in a zip). "
            u"<code>Ctrl+S</code>/<code>Ctrl+O</code>. “Reference” restores the textbook "
            u"103 km scenario.</p>"
            u"<h3>Export</h3>"
            u"<ul>"
            u"<li><b>KML</b> — path, sites and common volume for Google Earth;</li>"
            u"<li><b>Profile CSV</b> — distance/elevation/void columns;</li>"
            u"<li><b>Budget CSV</b> — the full waterfall and model table.</li>"
            u"</ul>");
    }
    if (t == QLatin1String("about")) {
        return QStringLiteral(
            u"<h3>TropoLink</h3>"
            u"<p>An instrument for designing troposcatter radio links. The program "
            u"version is shown in the title bar.</p>"
            u"<h3>Author</h3>"
            u"<p><b>Made by LukiBox.</b><br>"
            u"<a href=\"https://github.com/LukiBox\">https://github.com/LukiBox</a></p>"
            u"<h3>Licence</h3>"
            u"<p>TropoLink is released under the <b>Apache License 2.0</b> (see the "
            u"<code>LICENSE</code> file). Third-party components keep their own "
            u"licences — the full list is in <code>NOTICE</code> and "
            u"<code>docs/third_party.md</code>.</p>"
            u"<p>In particular, <b>Qt 6 is used under LGPL v3 and linked "
            u"dynamically</b>: the Qt libraries ship as separate DLLs beside the "
            u"executable and can be replaced, as LGPL v3 §4 requires.</p>"
            u"<p>The ITM (Longley-Rice) model is the official public-domain NTIA "
            u"implementation, vendored unmodified.</p>"
            u"<h3>Privacy</h3>"
            u"<p>No telemetry. The only network traffic that can occur is map or SRTM "
            u"downloading that the operator starts deliberately. The Air-Gap flavour "
            u"contains no network code at all.</p>");
    }
    return {};
}

} // namespace

QVariantList tropolinkHelpTopics(bool polish) {
    QVariantList list;
    for (const auto& t : kTopics) {
        list << QVariantMap{{QStringLiteral("id"), QString::fromUtf8(t.id)},
                            {QStringLiteral("title"),
                             QString::fromUtf8(polish ? t.titlePl : t.titleEn)}};
    }
    return list;
}

QString tropolinkHelpHtml(const QString& topic, bool polish) {
    return polish ? helpPl(topic) : helpEn(topic);
}
