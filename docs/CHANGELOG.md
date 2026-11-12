# Zmiany

## 0.3.0

### Naprawy (błędy znalezione przez uruchomienie kodu)
- Przywrócono zgubione pliki: `h2d_collision`, `h2d_audio`, `h2d_camera`, `h2d_anim`,
  `h2d_app` (silnik tekstowy), `native/build.sh`, `docs/LIMITATIONS.md`. W 0.2.0
  `h2d_audio.h#` i `h2d_collision.h#` zawierały kod silnika graficznego pod
  nazwami silnika tekstowego; przeniesiono je do `h2d_audio2.h#` i
  `h2d_collision2d.h#`, których oczekiwały `lib.h#` i Pong.
- `h2d_texture.h#`: wieloliniowe listy parametrów nie przechodzą parsera `h#`
  (blokowało animacje, encje, `h2d_app2d`, `lib.h#`).
- `h2d_pathfind`: wołał nieistniejące `queue_new()` i brał elementy z końca (DFS);
  teraz prawdziwa kolejka FIFO, więc ścieżka jest najkrótsza.
- `h2d_input` / Sokoban: `io_read_line()` → `io::read_line("")` (obecne std).
- Natywnie: `load_texture_from_rgba` czytał nagłówek `HshArray` jako piksele i
  używał `RGBA8888` (bajty A,B,G,R na little-endian) → poprawny odczyt `bytes` +
  `SDL_PIXELFORMAT_RGBA32`.
- Natywnie: wciśnięcie i puszczenie klawisza/przycisku w tej samej klatce ginęło
  (szybkie kliknięcie nie działało); teraz zbocza są zatrzaskiwane ze zdarzeń.
- Natywnie: brak zabezpieczeń `begin/end_frame` bez okna, zablokowane klawisze po
  utracie fokusa, nadpisywanie żywych dźwięków/muzyki w pełnej tablicy,
  `close_window` nieidempotentne, każdy tekst > 8 px tworzył i niszczył teksturę.
- `Bit.hk`: nowy format (`[edition] toolchain`, `[layout] tests/examples`,
  `[build] native`, `[commands]`), usunięto `[native-sources]` (dla H# nie działa),
  dodano SDL2_ttf.
- Demo przeniesione z `src/` do `examples/` (biblioteka nie powinna zawierać `main`).

### Nowe — natywne (`native/h2d_native.c`)
Pełny ekran, rozmiar/min. rozmiar okna, wirtualna rozdzielczość + skala całkowita,
kursor/relative mouse, ikona okna, `request_close`, FPS, tryby blendowania, clip,
piksel/elipsa/zaokrąglony prostokąt, `draw_texture_full`, filtr tekstur, render targety,
czcionki TrueType z cache'em, zrzut ekranu, F1–F12 i ~40 kolejnych klawiszy,
delta myszy, puszczenie przycisku, tekst Unicode, 4 pady, audio: pan/głośność/kanały,
pauza, fade, głośność główna, `unload_*`. Zmienne środowiskowe `H2D_MAX_FRAMES`, `H2D_SCREENSHOT`.

### Nowe — H#
`h2d_tiles2d`, `h2d_physics2d`, `h2d_particles`, `h2d_ui`, `h2d_scene`, `h2d_tween`,
`h2d_save`, `Animator`, `Cooldown`/`Timer`, `Rng`, wstrząs i płynna kamera, `compact`
dla encji, `separation_vector`/`segment_hits_rect`, `from_hex`/`from_hsv`/`lerp`
kolorów, `load_texture_from_art`, `new_app2d_virtual`, `move_axis`, `examples/platformer.h#`,
`tests/`, `scripts/run-example.sh`.

## 0.2.0
Pierwszy silnik graficzny (SDL2).
