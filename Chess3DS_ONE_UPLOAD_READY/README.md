# Chess3DS

Lekka, natywna gra szachowa dla Nintendo 3DS i 2DS. Interfejs jest w całości
dwuwymiarowy, a przeciwnikiem jest działający offline **Stockfish 11**. Projekt
jest celowo projektowany pod Old 3DS — bez ciężkiego widoku 3D i bez połączenia
z internetem.

## Funkcje

- pełne zasady szachów: roszada, en passant, promocja, szach, mat i pat;
- remisy przez trzykrotne powtórzenie, regułę 50 posunięć i brak materiału;
- Stockfish 11 z 8 poziomami trudności i asynchronicznym liczeniem ruchów;
- lokalna partia dla dwóch graczy na jednej konsoli;
- zegary: bez limitu, 10 minut, 5 minut oraz 3+2;
- sterowanie dotykiem, krzyżakiem i Circle Padem;
- podświetlanie legalnych ruchów, ostatniego ruchu i szachowanego króla;
- płynne animacje figur oraz syntetyzowane dźwięki bez dodatkowych plików;
- cztery motywy planszy: Klasyczny, Nocny, Leśny i Palisander;
- automatyczny zapis niedokończonej partii;
- automatyczny eksport zakończonych partii do PGN;
- statystyki gry ze Stockfishem;
- pięć wbudowanych zadań matowych;
- interfejs po polsku i angielsku.

## Sterowanie

| Przycisk | Działanie |
| --- | --- |
| A / ekran dotykowy | wybór figury i wykonanie ruchu |
| Krzyżak / Circle Pad | poruszanie kursorem |
| B / START | anulowanie wyboru lub pauza |
| Y | cofnięcie ruchu; przeciwko CPU cofa pełną parę ruchów |
| X | obrócenie planszy |
| L / R | zmiana opcji tam, gdzie jest dostępna |

## Pobieranie wersji do gry

1. Otwórz zakładkę **Actions** w tym repozytorium.
2. Wybierz najnowszy udany workflow **Build Chess3DS**.
3. Pobierz artefakt `Chess3DS-v1.0.1`.
4. Dla Homebrew Launchera skopiuj `Chess3DS.3dsx` i `Chess3DS.smdh` do
   `/3ds/Chess3DS/` na karcie SD.
5. Jeżeli artefakt zawiera `Chess3DS.cia`, można zamiast tego zainstalować tę
   wersję na konsoli z odpowiednio skonfigurowanym CFW.

Pliki ustawień, zapis i partie PGN powstają w `sdmc:/3ds/Chess3DS/`.

## Budowanie

Wymagane są aktualne pakiety `3ds-dev`, w tym devkitARM, libctru, citro2d i
citro3d. W powłoce devkitPro:

```sh
make -j2
```

Powstają `Chess3DS.3dsx` i `Chess3DS.smdh`. Opcjonalny cel `make cia`
wymaga programów `cxitool` oraz `makerom`.

Testy uruchamiane na komputerze:

```sh
make -f Makefile.host test
```

Testy rdzenia porównują generator ruchów z referencyjnymi wynikami perft,
w tym 197 281 pozycji ze startu na głębokości 4 oraz 97 862 pozycji Kiwipete na
głębokości 3. Test różnicowy porównuje wszystkie legalne ruchy z generatorem
Stockfisha w 3600 deterministycznie wylosowanych pozycjach. Osobny smoke test
sprawdza także rzeczywiste wyszukiwanie ruchu przez osadzony silnik.

## Wydajność

Stockfish używa jednego wątku, 2 MiB tablicy transpozycji i nie ładuje tablic
Syzygy. Jego wyszukiwanie działa poza pętlą renderowania, dlatego interfejs nie
czeka na zakończenie obliczeń. Wyższy poziom oznacza dłuższy czas liczenia;
numery 1–8 nie są obietnicą konkretnego rankingu Elo.

## Diagnostyka na konsoli

Wersja 1.0.1 ładuje dźwięk i Stockfisha dopiero wtedy, gdy są potrzebne, oraz
zapisuje etap działania do `/3ds/Chess3DS/last_stage.txt`. Plik jest usuwany po
normalnym zamknięciu. Jeżeli konsola pokaże wyjątek, przed ponownym uruchomieniem
skopiuj ten plik oraz najnowszy dump z `/luma/dumps/arm11/` do zgłoszenia błędu.

## Licencja

Cały projekt jest udostępniony na GNU GPL v3 lub nowszej, zgodnie z licencją
Stockfisha. Szczegóły i dokładna wersja kodu silnika znajdują się w
[THIRD_PARTY.md](THIRD_PARTY.md). Odpowiadający kod źródłowy każdej kompilacji
jest dostępny w tym repozytorium.

Nintendo 3DS jest znakiem towarowym Nintendo. Projekt nie jest powiązany ani
zatwierdzony przez Nintendo.
