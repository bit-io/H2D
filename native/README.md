# native/ — H2D's native graphics/audio/input backend

`h2d_native.c` is a small C shim around SDL2 + SDL2_image + SDL2_mixer +
SDL2_gfx. It gives H#'s `extern [c]` FFI (which can only cross the
language boundary with plain scalars — see the ABI note at the top of
`h2d_native.c`) a flat, handle-based API to link against, instead of trying
to expose SDL2's real struct-heavy API directly.

`src/h2d_gfx.h#`, `src/h2d_texture.h#`, `src/h2d_input2d.h#` and
`src/h2d_audio2.h#` are the H# side: each has an `extern dynamic [c,
"h2d_native"] is ... end` block declaring exactly the functions this shim
exports, plus a thin `pub fn` wrapper per function for a nicer call
convention (structs instead of four separate r/g/b/a ints, `bool` instead
of `int`, etc).

## Build

```bash
./native/build.sh
```

Needs `gcc` (or another C compiler via `CC=...`), `pkg-config`, and the dev
packages for SDL2, SDL2_image, SDL2_mixer, SDL2_gfx:

```bash
# Debian/Ubuntu
sudo apt-get install libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-gfx-dev

# Fedora
sudo dnf install SDL2-devel SDL2_image-devel SDL2_mixer-devel SDL2_gfx-devel

# Arch
sudo pacman -S sdl2 sdl2_image sdl2_mixer sdl2_gfx

# macOS (Homebrew)
brew install sdl2 sdl2_image sdl2_mixer sdl2_gfx
```

Produces `build/libh2d_native.so` (`.dylib` on macOS, `.dll` on Windows —
see `build.sh`). `bytes build`/`bytes run` run this automatically as a
pre-build step once you add the `[native]` section shown in this project's
`Bytes.hk`; run it by hand any time you just want to check the shim
compiles after an edit.

Then compile your game against it the normal H# way, e.g.:

```bash
h# compile src/pong_main.h# -o pong -L build -lh2d_native
h# run ./pong
```

(exact linker-flag spelling for `h# compile` may differ slightly from
this — check `h# compile --help` / `docs/HLIB_FORMAT.md` in the H#
repo for how `extern dynamic [c, "name"]` resolves a library name to a
`-l`/search-path pair on your version; this project's `Bytes.hk` is the
source of truth once you've confirmed it there.)

## What was actually verified, and how

This sandbox has real SDL2 2.30.0, SDL2_image 2.8.2, SDL2_mixer 2.8.0, and
SDL2_gfx 1.0.2 dev packages (installed from Ubuntu 24.04's `universe`
repo) but no display, no audio device, no LLVM 21, and therefore no way to
build the actual `h#` compiler binary from `H-Sharp-main/source-code`. So
verification split into two halves:

**The C shim (`h2d_native.c`) — compiled and run, not just read.**
`gcc -Wall -Wextra -fPIC -shared` against the real headers above,
zero warnings. Then loaded with `dlopen`/`dlsym` from a small C harness
(mirroring exactly how a real `h#`-compiled binary would call into it —
same function pointer types, same calling convention) and driven through
SDL's headless `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` backends:

- opened a window, ran several `begin_frame`/`end_frame` cycles,
  confirmed `get_frame_time()` reflects the ~16.6ms 60fps cap
- loaded a real PNG (`h2d_load_texture`) and drew it rotated via
  `h2d_draw_texture_ex` (`SDL_RenderCopyEx`)
- drew rectangles, text (the SDL2_gfx bitmap font)
- initialized the audio device (`h2d_audio_init`) and confirmed a
  bogus sound path fails cleanly (returns handle `0`) rather than
  crashing
- closed everything down without leaking (no ASan run in this pass, but
  every allocate path has a matching free path — see `h2d_close_window`)

**The H# side (`h2d_gfx.h#` and friends) — written against, not run
through, the compiler.** Every `extern` signature was checked by hand
against `compiler::ffi::named_to_c`'s documented mapping (H# `int` →
C `int64_t`, `f64` → `double`, `bool` → C `int32`, `string` → `const
char*`, `bytes` → `uint8_t*` — see `source-code/compiler/src/ffi.rs`),
and every `.h#` file's syntax was checked by hand against working
examples already in this repo/H# repo (`examples/showcase.h#`'s `extern
dynamic [c]` blocks, `tests/compiler/unsafe_arena_kinds_test.h#`'s
`extern static [c]`, `std/money.h#`/`std/color.h#`'s `(n + 0.0)`
int→float idiom, `h2d_menu.h#`'s `if cond then a else b` expression
syntax). It has **not** been run through `h# check` or `h# compile`
against a real H# toolchain — do that before shipping a game on top of
this. If something doesn't compile, it's almost certainly a small
mismatch in this newer H#-side code, not in `h2d_native.c` (which is
real, compiled, working C).

## Adding a function

1. Add it to `h2d_native.c`, remembering the ABI rule: any H# `int` is
   `int64_t` here, `f64` is `double`, `bool` return values are avoided
   (return `int64_t` 0/1 instead, so the boundary only ever has two
   integer widths to keep straight — see the comment block at the top
   of the file).
2. Add the matching line to the relevant `extern dynamic [c,
   "h2d_native"] is ... end` block in the `.h#` file it belongs to.
3. Add a `pub fn` wrapper with the nicer H2D-style call convention
   (a `Color`/`Vec2`/etc struct instead of loose scalars, `bool`
   instead of `int`) — don't expose the raw `extern fn` to game code
   directly, exactly as every existing function in these files does.
4. Rebuild (`./native/build.sh`) and re-run (or extend) the smoke test
   described above before assuming it works.


## 0.3.0 notes

- **Dependencies:** SDL2, SDL2_image, SDL2_mixer, SDL2_gfx and **SDL2_ttf**
  (`bit system check` verifies them). `build.sh` produces the *static archive*
  `build/libh2d_native.a`; the game links it with `LIBRARY_PATH=build h# compile … --dynamic`.
  `h# compile` has no `-L`/`-l` flags (the old instructions were wrong).
- **ABI rule:** H# `int` = `int64_t`, `f64` = `double`, `string` = `const char*`. **`bytes` is
  not a raw buffer**: at runtime it is an `HshArray*` (`len, cap, int64_t *data, own`, one `int64`
  per byte). `h2d_load_texture_from_rgba` flattens it; copy that pattern for any new `bytes` param.
- **Environment variables:** `H2D_MAX_FRAMES=N` quits after N frames; `H2D_SCREENSHOT=file.png`
  saves the last frame as PNG. Combined with `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` this is
  the headless test mode used by `tests/smoke.sh`.
- Input edges (pressed/released) are latched from the event queue, so a click that lasts less
  than one frame is still seen.
