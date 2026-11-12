# Ograniczenia i status weryfikacji

Ten plik jest celowo szczery o tym, co w H2D zostało realnie uruchomione i
sprawdzone, a co jest napisane poprawnie *wedle wiedzy o H#*, ale nie
przepuszczone przez prawdziwy kompilator `h#`. Przeczytaj to przed
budowaniem na tym czegoś większego.

## Silnik graficzny (`h2d_gfx`, `h2d_texture`, `h2d_input2d`, `h2d_audio2`, ...)

**Co jest prawdziwe:** to nie atrapa. `native/h2d_native.c` to realny plik
C, skompilowany w tym środowisku przeciwko prawdziwym nagłówkom SDL2 2.30 /
SDL2_image 2.8 / SDL2_mixer 2.8 / SDL2_gfx 1.0, i **uruchomiony** (nie
tylko skompilowany) przez `dlopen`/`dlsym` z headless backendem SDL
(`SDL_VIDEODRIVER=dummy`, `SDL_AUDIODRIVER=dummy`):

- otwarcie okna, pętla `begin_frame`/`end_frame`, limiter FPS — działa
- wczytanie prawdziwego PNG i narysowanie go obróconego
  (`SDL_RenderCopyEx`) — działa
- prymitywy (prostokąt, koło, linia, trójkąt, tekst z wbudowanej
  bitmapowej czcionki SDL2_gfx) — działa
- `h2d_audio_init` na prawdziwym (choć niemym) urządzeniu audio — działa
- błędne ścieżki/uchwyty (zły plik, uchwyt 0) — czysty no-op, nie crash

Dokładne polecenia weryfikacji są w `native/README.md`.

**Czego NIE zweryfikowano:** strona H# (`src/h2d_gfx.h#`,
`h2d_texture.h#`, `h2d_input2d.h#`, `h2d_audio2.h#`, `h2d_camera2d.h#`,
`h2d_entity2d.h#`, `h2d_anim2d.h#`, `h2d_collision2d.h#`, `h2d_app2d.h#`,
`h2d_color.h#`, `h2d_keys.h#`, `pong_main.h#`) — bo to środowisko nie ma
toolchaina LLVM 21 potrzebnego do zbudowania samego kompilatora `h#`
(`source-code/`), więc nie da się tu uruchomić `h# check`/`h# compile`.
Każda deklaracja `extern` została ręcznie zweryfikowana względem
udokumentowanego mapowania typów w `compiler::ffi::named_to_c`
(`source-code/compiler/src/ffi.rs`) — H# `int` → C `int64_t`, `f64` →
`double`, `bool` → C `int`, `string` → `const char*`, `bytes` →
`uint8_t*` — a cała reszta składni względem działających przykładów już
obecnych w repo H# (`examples/showcase.h#`, `tests/compiler/*.h#`,
`std/money.h#`, `std/color.h#`, `h2d_menu.h#`). Zanim zbudujesz na tym coś
większego: uruchom `h# check` na każdym nowym pliku, potem `h# compile
src/pong_main.h#` i realnie zagraj w Ponga. Jeśli coś nie zadziała tak, jak
opisano — to niemal na pewno literówka/nieporozumienie po stronie tego
nowego kodu H#, nie po stronie `h2d_native.c` (który jest realny i
przetestowany).

**Realne ograniczenia projektowe** (nie "jeszcze nie sprawdzone", tylko
"tak to działa"):
- FFI H# nie przenosi structów przez wartość (`StructByValueFfi`), więc
  cała komunikacja z SDL2 idzie przez płaskie skalary — stąd uchwyty
  (int) zamiast wskaźników, i stąd `draw_rectangle` bierze cztery
  osobne inty r/g/b/a zamiast jednego `SDL_Color`. `h2d_gfx.h#` i
  reszta chowają to za wygodniejszym `pub fn` API (przyjmującym
  `h2d_color::Color`), ale bezpośrednio pod spodem to wciąż płaskie
  wywołania C.
- Wbudowany tekst to bitmapowa czcionka 8×8 z SDL2_gfx — bez kerningu,
  bez Unicode poza ASCII, bez TrueType. Do prawdziwej typografii:
  wczytaj własną czcionkę jako spritesheet i rysuj przez
  `h2d_texture::draw_texture_rec` (to właśnie robi `h2d_anim2d`).
- `std -> image` (wbudowany loader H#) dekoduje tylko nieskompresowany
  24-bit BMP (patrz `std/image.h#`) — ale silnik graficzny go nie
  potrzebuje: `h2d_texture::load_texture` idzie przez SDL_image (PNG,
  JPG, BMP, GIF, ...) bezpośrednio z dysku, z realną dekompresją.
- Dźwięk przez `h2d_audio2` wymaga realnego (lub `dummy`) urządzenia
  audio dostępnego dla SDL — w pełni bezgłowym środowisku bez ŻADNEGO
  audio backendu `audio_init()` zwróci `false`, a każda kolejna
  funkcja w module jest wtedy bezpiecznym no-opem (gra działa dalej,
  po prostu bez dźwięku).
- Brak własnego systemu cząsteczek/shaderów/warstw post-processingu —
  to prosty immediate-mode renderer 2D (rect/circle/line/texture),
  celowo blisko API macroquad, nie silnik z shaderami czy sceną 3D.

## Silnik tekstowy (`h2d_canvas`, `h2d_entity`, `h2d_tilemap`, ...)

Bez zmian względem wcześniejszej wersji tego pliku (a właściwie: bez
zmian względem README's poprzedniej sekcji "Znane ograniczenia" — ten
plik po prostu teraz istnieje naprawdę zamiast być tylko odnośnikiem bez
celu):

- Brak grafiki pikselowej/okna w tym silniku z założenia — to
  ANSI truecolor w terminalu, i to jest cały jego sens (gry turowe:
  roguelike, sokobany, strategie, karcianki).
- Brak wejścia w czasie rzeczywistym w tym silniku — `h2d_input` czyta
  całą linię (Enter kończy ruch), bo to silnik turowy z założenia. Do
  czasu rzeczywistego służy teraz drugi silnik (`h2d_input2d`, wyżej).
- `h2d_audio` w tym silniku zostaje uczciwą atrapą (loguje, nic nie
  gra) — bo nie ma czego tu owijać: terminal nie odtwarza dźwięku, a
  prawdziwy dźwięk (`h2d_audio2`) należy do silnika graficznego z jego
  własnym oknem/urządzeniem audio, nie do tego modułu.
- Kod tego silnika był już wcześniej napisany względem źródeł
  interpretera/kompilatora H#, ale — tak jak nowy kod graficzny
  powyżej — nigdy nie przepuszczony przez realny `h#` w tym
  środowisku (brak pełnego środowiska budowania z LLVM 21). Ta część
  się nie zmieniła.
