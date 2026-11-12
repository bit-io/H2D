# Jak napisać grę 2D jako aplikację w H2D

## 1. Układ projektu

```
moja-gra/
  src/            ← skopiuj tu src/*.h# z H2D (albo trzymaj H2D obok)
  gra.h#          ← twój plik z fn main()
  native/         ← h2d_native.c + build.sh z H2D
  assets/         ← obrazki, dźwięki, czcionki (opcjonalnie)
  scripts/run-example.sh   ← wzór budowania, zmień nazwę przykładu na swoją
```
Budowanie (to samo robi `scripts/run-example.sh`):
```bash
bash native/build.sh                              # build/libh2d_native.a
LIBRARY_PATH=build h# compile gra.h# --dynamic -o gra
./gra                                             # uruchamiaj z katalogu gry (ścieżki assets/…)
```
Powód `--dynamic` i `LIBRARY_PATH`: `docs/LIMITATIONS.md`, pkt 1–2.

## 2. Szkielet gry

```h#
mod h2d_gfx
mod h2d_color

fn main() is
    h2d_gfx::init_window(960, 540, "Gra")
    h2d_gfx::set_target_fps(60)
    h2d_gfx::set_logical_size(320, 180)        ;; opcjonalnie: wirtualna rozdzielczość (pixel art)
    while !h2d_gfx::window_should_close() is
        let dt: f64 = h2d_gfx::get_frame_time()
        ;; 1. wejście   2. aktualizacja (dt)   3. rysowanie
        h2d_gfx::clear_background(h2d_color::black())
        h2d_gfx::next_frame()
    end
    h2d_gfx::close_window()
end
```

## 3. Który moduł do czego

| Potrzebujesz | Użyj |
|---|---|
| Poziom z kafelków | `h2d_tiles2d::from_rows("…", legenda)` lub `from_csv` (Tiled); `set_solid`; `draw_flat` / `draw` |
| Postać chodząca po poziomie | `h2d_physics2d::Body`: `apply_gravity`, `move_body`, `jump` |
| Animowana postać | `h2d_anim2d::Animator` (`add_clip`, `play`, `animator_update`, `animator_draw`) |
| Grafika bez plików | `h2d_texture::load_texture_from_art(rows, znaki, paleta)` |
| Kamera za graczem | `h2d_camera2d::follow_smooth`, `shake`, `update` |
| Efekty | `h2d_particles::burst` / `cone`, `h2d_tween` |
| Menu, HUD, ustawienia | `h2d_ui::button`, `menu`, `slider`, `checkbox`, `progress_bar` |
| Ekrany gry (menu → gra → koniec) | `h2d_scene::Scenes` + `change` |
| Rekordy, ustawienia | `h2d_save::load` / `set_int` / `commit` |
| Polskie litery | `h2d_gfx::load_font("plik.ttf", 14)` + `draw_text_font` (lub `h2d_ui::with_font`) |
| Powtarzalna losowość | `h2d_random::new_rng(ziarno)` |
| Dźwięk | `h2d_audio2::audio_init`, `load_sound`, `play_sound_ex`, `load_music` |

## 4. Wzorzec: wartość w, wartość out

```h#
g.hero = h2d_physics2d::apply_gravity(g.hero, 700.0, 380.0, dt)
g.hero = h2d_physics2d::move_body(g.hero, g.map, dt)
```
Każda funkcja zwraca zmieniony egzemplarz; przypisz go z powrotem.

## 5. Dobre praktyki

- Stały krok fizyki nie jest potrzebny: `move_body` dzieli ruch na podkroki (bez tunelowania),
  a `dt` jest obcięty do 0,25 s.
- Zwalniaj zasoby (`unload_texture`, `unload_font`, `unload_sound`), jeśli ładujesz je w trakcie gry.
- Encje: po klatce wywołaj `h2d_entity2d::compact`, inaczej martwe zostają w pamięci.
- Test bez okna: `H2D_MAX_FRAMES=120 H2D_SCREENSHOT=klatka.png SDL_VIDEODRIVER=dummy ./gra`.
- Kompletny wzór: `examples/platformer.h#`.
