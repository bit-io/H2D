# Ograniczenia i status weryfikacji (0.3.0)

Ten plik mówi, co zostało naprawdę uruchomione, a co jest ograniczeniem
na stałe lub do zrobienia. Wszystko poniżej sprawdzono prawdziwymi
narzędziami: `h#` 0.9.0, `bit` 0.3.0, gcc, SDL2 2.30 / SDL2_image 2.8 /
SDL2_mixer 2.8 / SDL2_gfx 1.0 / SDL2_ttf 2.22, Xvfb (prawdziwy serwer X11).

## Co zweryfikowano

| Element | Jak |
|---|---|
| Wszystkie moduły `src/*.h#` | `h# check` bez błędów |
| Logika bez okna (wektory, kolory, tween, RNG, timery, mapy, fizyka, kolizje, sceny, zapis, cząsteczki, kamera, silnik tekstowy) | `bit test` → 15 testów, 0 błędów (`tests/logic_test.h#`) |
| Natywny backend `native/h2d_native.c` | `gcc -Wall -Wextra` bez ostrzeżeń |
| Pong, platformówka | kompilacja `h# compile --dynamic`, uruchomienie pod Xvfb (X11) z prawdziwym oknem, kliknięcie myszą, sterowanie klawiaturą, zrzut ekranu obejrzany |
| Tryb headless | `SDL_VIDEODRIVER=dummy` — `tests/smoke.sh` |
| Sokoban (silnik tekstowy) | kompilacja i uruchomienie w terminalu |
| `Bit.hk` | `bit publish`, `bit system check`, komendy `[commands]` |

## Czego NIE sprawdzono

- **Dźwięku na prawdziwym urządzeniu** — tylko sterownik `dummy` (ścieżki
  wywołań działają, brak nasłuchu). Test: `bit pong`, podłożyć `assets/bounce.wav`.
- **Pada** — kod obsługi zdarzeń napisany według dokumentacji SDL, bez fizycznego
  urządzenia nie testowany.
- **Windows / macOS / Wayland** — tylko Linux/X11. SDL2 jest wieloplatformowy, ale
  `native/build.sh` i `scripts/run-example.sh` są skryptami bash.
- **Akcelerowanego GPU** — Xvfb używa renderera programowego.

## Ograniczenia na stałe i znane braki

1. **`bit build` nie zbuduje gry z H2D.** `bit` linkuje wszystko statycznie i
   odrzuca `extern dynamic` (`bit publish`: „uses `extern dynamic`”). SDL2 nie da
   się zlinkować statycznie: próba `gcc -static` kończy się brakiem `libasound.a`,
   a statyczny glibc nie ma `dlopen`, którego SDL potrzebuje do sterowników okna.
   Obejście: `scripts/run-example.sh` (`h# compile --dynamic`, archiwum natywne
   statycznie, SDL przez pkg-config). **Poprawka po stronie `bit`** (nie w tym
   repo): zezwolić na `[build] link => dynamic` albo na `extern dynamic` dla
   bibliotek z `[system-dependencies]`. Do tego czasu `bit publish` dla h2d
   zgłasza ten jeden błąd, a poza nim Bit.hk jest poprawny.
2. **`h#` linkuje bibliotekę natywną tylko po nazwie z `LIBRARY_PATH`.** Stare
   instrukcje `-L build -lh2d_native` nie działają (`h# compile` nie ma takich
   flag). Używaj `LIBRARY_PATH=build`, jak robią skrypty.
3. **`mod x` szuka modułu obok kompilowanego pliku.** Dlatego skrypty kopiują
   `src/*.h#` razem z przykładem/testem do katalogu tymczasowego. W grze własnej:
   skopiuj `src/` do jej katalogu albo zrób to samo skryptem.
4. **Bez cieniowania/shaderów, bez cienkich linii z antyaliasingiem o grubości > 1,
   bez rysowania wielokątów wklęsłych** — tylko prymitywy SDL2_gfx i tekstury.
5. **Tekst bitmapowy 8×8 nie ma polskich liter.** Do polskiego tekstu użyj
   `h2d_gfx::load_font("…ttf", rozmiar)`. H2D nie dołącza żadnej czcionki;
   wskaż własny plik `.ttf` (np. DejaVu: `/usr/share/fonts/truetype/dejavu/`).
6. **Kolizje to AABB po siatce kafelków.** Brak ukośnych stoków, platform
   jednostronnych, kolizji obracanych prostokątów i symulacji bryły sztywnej.
7. **Brak sieci, brak ładowania map Tiled w formacie TMX/JSON** (jest CSV warstwy:
   `h2d_tiles2d::from_csv`), brak emulacji dotyku, brak shaderów, brak wielu okien.
8. **FFI H# nie przenosi struktur** — natywne API jest płaskie (liczby, napisy).
   `bytes` to w H# `HshArray*` z jednym `int64` na bajt, nie surowy bufor; shim
   spłaszcza go sam (w 0.2.0 był to błąd: tekstury z `load_texture_from_rgba`
   były śmieciami).
9. **Uchwyty zasobów są małymi liczbami w stałych tablicach** (tekstury, dźwięki,
   fonty: limity w `native/h2d_native.c`). Przy ładowaniu „w pętli” zwalniaj
   (`unload_*`), inaczej tablica się zapełni (wtedy `load_*` zwraca uchwyt 0).
10. **`delta time` jest obcięty do 0,25 s**, żeby przeciągnięcie okna nie wystrzeliło
    fizyki.

## Naprawione względem 0.2.0 (znalezione przy uruchomieniu)

Szczegóły w `docs/CHANGELOG.md`: ucięty zestaw plików (brakujące moduły silnika
tekstowego i `build.sh`), błąd składni w `h2d_texture.h#`, nieistniejące
`queue_new()` i DFS zamiast BFS w `h2d_pathfind`, błędne `io_read_line`,
zły format pikseli i zły odczyt `bytes` w teksturach, gubienie szybkich kliknięć
i klawiszy w obrębie jednej klatki, przypadkowe nadpisywanie dźwięków w pełnej tablicy,
`extern dynamic` jako jedyny tryb.
