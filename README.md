# H2D

Biblioteka do pisania gier 2D w **H#**, dla menedżera pakietów **bit** (`Bit.hk`).

H2D ma dwa silniki:

- **Silnik graficzny** (`h2d_gfx` i moduły pokrewne) — prawdziwe okno
  systemowe, renderowanie GPU (SDL2), tekstury PNG/JPG/BMP/GIF, czcionki
  TrueType (UTF-8, polskie znaki), wejście w czasie rzeczywistym
  (klawiatura, mysz, **pad**, wpisywany tekst), prawdziwy dźwięk, wirtualna
  rozdzielczość i pełny ekran. API blisko [macroquad](https://macroquad.rs)'a.
  **To jest silnik do gier-aplikacji.**
- **Silnik tekstowy** (`h2d_canvas` i moduły pokrewne) — siatka znaków w
  terminalu (ANSI truecolor), dla gier turowych (roguelike, sokoban,
  karcianki). Nie mieszaj go z silnikiem graficznym w jednej grze.

Wersja **0.3.0** domyka silnik graficzny do kompletnych gier: mapy kafelkowe
z fizyką, animacje z wieloma klipami, cząsteczki, UI, sceny z przejściami,
zapis gry, tweeny, generator losowy z ziarnem. Przewodnik krok po kroku:
**`docs/GUIDE.md`**. Co działa, co sprawdzono i co jest ograniczeniem:
**`docs/LIMITATIONS.md`**. Zmiany: `docs/CHANGELOG.md`.

## Szybki start

```bash
# 1. zależności systemowe (SDL2 + obrazy, dźwięk, prymitywy, TTF)
sudo apt-get install libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-gfx-dev libsdl2-ttf-dev
bit system check                 # sprawdza je wg [system-dependencies] z Bit.hk

# 2. zbuduj i uruchom demo
bit platformer                   # = scripts/run-example.sh platformer
bit pong
bit sokoban                      # silnik tekstowy, w terminalu

# 3. testy
bit test                         # logika bez okna (15 testów)
bash tests/smoke.sh              # przykłady headless (SDL dummy), bez ekranu
```

> **Ważne — budowanie własnej gry.** SDL2 nie da się zlinkować statycznie
> (wymaga `dlopen` sterowników okna/dźwięku), a `bit build` odrzuca
> `extern dynamic`. Dlatego gry z H2D buduje się dziś przez `h#` z flagą
> `--dynamic` — robi to `scripts/run-example.sh` (skopiuj go do swojego
> projektu). Szczegóły i proponowana poprawka po stronie `bit`:
> `docs/LIMITATIONS.md`, punkt 1.

Minimalna gra (kwadrat sterowany WASD / strzałkami / padem):

```h#
mod h2d_gfx
mod h2d_color
mod h2d_input2d

fn main() is
    h2d_gfx::init_window(640, 480, "Moja gra")
    h2d_gfx::set_target_fps(60)
    let mut x: f64 = 300.0
    let mut y: f64 = 200.0

    while !h2d_gfx::window_should_close() is
        let dt: f64 = h2d_gfx::get_frame_time()
        let a = h2d_input2d::move_axis()        ;; WASD + strzałki + pad, znormalizowane
        x += a.0 * 220.0 * dt
        y += a.1 * 220.0 * dt

        h2d_gfx::clear_background(h2d_color::dark_blue())
        h2d_gfx::draw_rectangle(x, y, 40.0, 40.0, h2d_color::gold())
        h2d_gfx::next_frame()
    end
    h2d_gfx::close_window()
end
```

Uruchamianie bez ekranu (CI, testy): `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
H2D_MAX_FRAMES=120 H2D_SCREENSHOT=klatka.png ./gra` — zamyka grę po N klatkach i
zapisuje PNG ostatniej klatki.

## Moduły — silnik graficzny

| Moduł | Do czego |
|---|---|
| `h2d_gfx` | Okno, pętla klatek, prymitywy (prostokąt, koło, elipsa, zaokrąglony prostokąt, linia, trójkąt, piksel), **tekst bitmapowy i TrueType**, **pełny ekran, wirtualna rozdzielczość, clip, tryby blendowania, zrzut ekranu, FPS** |
| `h2d_color` | `Color` + paleta; **`from_hex`, `from_hsv`, `lerp`, `lighten`, `darken`** |
| `h2d_texture` | `Texture2D`: PNG/JPG/BMP/GIF, obrót/skala/flip/tint, wycinki, **`draw_texture_full`, tekstury generowane z kodu (`load_texture_from_art`), render targety, filtr nearest/linear** |
| `h2d_input2d` | Klawiatura, mysz (**delta, puszczenie**), **pad (4), wpisywany tekst Unicode, „dowolny klawisz”, `move_axis`** |
| `h2d_keys` | Kody klawiszy: litery, cyfry, strzałki, **F1–F12, Alt, Del, Home/End, PgUp/PgDn, interpunkcja, numpad** |
| `h2d_audio2` | Efekty i muzyka (SDL2_mixer); **głośność/pan, pętle z kanałem, pauza, fade, głośność główna, zwalnianie** |
| `h2d_camera2d` | Kamera (target/offset/zoom); **wstrząs, płynne śledzenie, zoom, culling** |
| `h2d_entity2d` | Encje pikselowe; **`compact`, kolizje encji, rysowanie przez kamerę** |
| `h2d_anim2d` | Animacja spritesheet'a; **`Animator` z nazwanymi klipami (idle/run/jump) i flipem** |
| `h2d_collision2d` | AABB/koło; **wektor separacji, odcinek-prostokąt, punkt-w-kole** |
| `h2d_app2d` | Opakowanie okno + `World2D` + `Camera2D`; **`new_app2d_virtual`** |
| `h2d_tiles2d` *(nowy)* | Mapy kafelkowe: z tekstu lub CSV z Tiled, tileset/paleta, solid, rysowanie z cullingiem |
| `h2d_physics2d` *(nowy)* | Ciała AABB po mapie kafelkowej: grawitacja, skok, coyote time, bez tunelowania |
| `h2d_particles` *(nowy)* | System cząsteczek: wybuch, stożek, grawitacja, tarcie, blending addytywny |
| `h2d_ui` *(nowy)* | UI natychmiastowe: przycisk, checkbox, suwak, pasek, panel, menu — z TTF lub bitmapą |
| `h2d_scene` *(nowy)* | Sceny (menu/gra/koniec) z przejściami fade |
| `h2d_tween` *(nowy)* | 13 krzywych easing + `Tween` |
| `h2d_save` *(nowy)* | Trwały zapis `klucz=wartość` (ustawienia, rekordy) |

Przykłady: `examples/platformer.h#` (pełna gra używająca wszystkiego powyżej,
bez plików graficznych), `examples/pong.h#`.

## Moduły — silnik tekstowy

| Moduł | Do czego |
|---|---|
| `h2d_canvas` | Bufor ekranu (siatka znaków), tekst/prostokąty/ramki/sprite'y, redraw tylko zmian |
| `h2d_ansi` | Kolory 24-bit, pogrubienie, paleta |
| `h2d_sprite` | ASCII-art jako sprite'y |
| `h2d_anim` | Animacje klatkowe sterowane czasem (ms) |
| `h2d_camera` | Przewijany viewport na siatce |
| `h2d_entity` | Lekki ECS na siatce |
| `h2d_tilemap` | Statyczna geometria poziomu jako tablica stringów |
| `h2d_collision` | AABB, punkt-w-prostokącie, Manhattan, sąsiedzi kratowi |
| `h2d_pathfind` | BFS po tilemapie (**naprawiony: prawdziwa kolejka FIFO**) |
| `h2d_input` | Wejście turowe (wpisz ruch, Enter) |
| `h2d_menu` | Proste menu tekstowe |
| `h2d_audio` | Atrapa dźwięku dla terminala |
| `h2d_app` | `Canvas + World + Clock + running` |

Przykład: `examples/sokoban.h#` + `examples/sokoban_levels.h#`.

## Moduły wspólne

`h2d_vec2` (wektory; **obrót, kąt, lerp, `floor_int`**), `h2d_timer` (zegar, limiter,
**`Cooldown`, `Timer`**), `h2d_random` (CSPRNG systemowy; **`Rng` z ziarnem — powtarzalne
poziomy i powtórki**).

## Wzorzec API

Funkcje H2D biorą wartość i zwracają nową: `world = h2d_entity2d::integrate_all(world, dt)`.
Tak w H# wygląda mutacja pól struktury przez zmienną — żadna funkcja nic nie zmienia
„w tle” bez przypisania wyniku. Dotyczy obu silników.

## Licencja

MIT — patrz `LICENSE`.
