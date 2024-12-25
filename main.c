#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_rwops.h>
#include <SDL2/SDL_stdinc.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define MAP_WIDTH 10
#define MAP_HEIGHT 10
#define MAP_TILE_SIZE 100

#define PI 3.141592
#define is ==
typedef int32_t i32;
typedef uint32_t u32;
typedef float f32;
typedef double f64;

typedef struct {
  f64 x;
  f64 y;
} v2;

typedef struct {
  i32 x;
  i32 y;
} v2i;

typedef enum { EMPTY_PLACE, WALL } map_info;

typedef struct {
  bool is_running;
  u32 pixels[SCREEN_HEIGHT][SCREEN_WIDTH];
  u32 map[MAP_HEIGHT][MAP_WIDTH];
  v2 player_pos;
  f64 fov_angle;
} Game;

void init_sdl() {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "could not initialize sdl2: %s\n", SDL_GetError());
    exit(1);
  }
}

#define TEXTURE_WIDTH 8
#define TEXTURE_HEIGHT 8

u32 cool_texture[TEXTURE_WIDTH][TEXTURE_WIDTH] = {
    {0, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 1, 0, 0, 1, 1, 0},
    {0, 1, 1, 0, 0, 1, 1, 0}, {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 0, 0, 0, 0, 1, 0}, {0, 0, 1, 0, 0, 1, 0, 0},
    {0, 0, 0, 1, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},
};

f64 cast_single_ray(Game game, f64 angle) {
  i32 iter = 0;

  f64 x = game.player_pos.x;
  f64 y = game.player_pos.y;

  f64 dx = cos(angle);
  f64 dy = sin(angle);
  f64 delta = 0.1f;

  while (iter < 100) {
    x += dx * delta;
    y += dy * delta;
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) {
      break; // Exit if the ray goes out of bounds
    }

    if (game.map[(int)floor(y)][(int)floor(x)] == WALL) {
      f64 dist_square = (x - game.player_pos.x) * (x - game.player_pos.x) +
                        (y - game.player_pos.y) * (y - game.player_pos.y);
      return sqrt(dist_square);
    }

    iter++;
  }

  return -1;
}

Uint32 rgba_color(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  return SDL_MapRGBA(SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888), r, g, b, a);
}

void update_game(Game game, SDL_Texture *texture) {
  u32 pixels[SCREEN_HEIGHT][SCREEN_WIDTH] = {0};

  f64 fov = PI / 2;
  i32 ray_count = 160;
  f64 angle_step = fov / ray_count;

  Uint32 floor_color = rgba_color(255, 255, 255, 255);

  for (int j = SCREEN_HEIGHT / 2; j < SCREEN_HEIGHT; j += 5) {
    for (int k = 0; k < SCREEN_WIDTH; k += 5) {
      pixels[j][k] = floor_color;
    }
  }

  for (i32 i = 0; i < ray_count; i++) {
    f64 dist =
        cast_single_ray(game, game.fov_angle - (fov / 2) + i * angle_step);
    if (dist == -1) {
      continue;
    }

    i32 wall_width = SCREEN_WIDTH / ray_count;
    i32 wall_height = 300.0f / dist;

    if (dist < 1.5f) {
      dist = 1.5f;
      wall_height = 300.0f / dist;
    }

    Uint8 opacity = 255 - (dist * 10);
    Uint32 wall_color = rgba_color(0, opacity, 0, 255);
    Uint32 wall_color2 = rgba_color(opacity, 0, 0, 255);

    for (u32 j = SCREEN_HEIGHT / 2 - wall_height;
         j <= SCREEN_HEIGHT / 2 + wall_height; j++) {
      for (u32 k = wall_width * i; k <= wall_width * (i + 1); k++) {

        i32 texture_x = (i / 8) % TEXTURE_WIDTH;
        i32 texture_y = (j / 8) % TEXTURE_HEIGHT;

        if (cool_texture[texture_y][texture_x] == 0) {
          pixels[j][k] = wall_color;
        } else {
          pixels[j][k] = wall_color2;
        }
      }
    }
  }
  SDL_UpdateTexture(texture, NULL, pixels, SCREEN_WIDTH * sizeof(u32));
}

int main(int argc, char *args[]) {
  SDL_Window *window = SDL_CreateWindow("raycaster", SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH,
                                        SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
  if (window == NULL) {
    fprintf(stderr, "could not create window: %s\n", SDL_GetError());
    return 1;
  }

  SDL_SetRelativeMouseMode(true);

  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  Game game = (Game){.is_running = true,
                     .pixels = {0},
                     .map = {{1, 1, 1, 1, 0, 1, 0, 0, 0, 0},
                             {1, 0, 1, 0, 1, 0, 1, 1, 1, 1},
                             {1, 0, 0, 0, 0, 0, 0, 1, 1, 1},
                             {1, 0, 0, 0, 0, 0, 0, 0, 1, 1},
                             {1, 1, 0, 0, 0, 0, 0, 0, 1, 1},
                             {1, 0, 0, 1, 1, 1, 1, 0, 1, 1},
                             {1, 0, 0, 0, 0, 0, 1, 0, 1, 0},
                             {1, 0, 0, 0, 1, 1, 1, 0, 1, 0},
                             {1, 0, 0, 0, 0, 0, 0, 0, 1, 0},
                             {1, 1, 1, 1, 1, 1, 1, 1, 1, 0}},
                     .player_pos = (v2){.x = 4.0f, .y = 4.0f},
                     .fov_angle = 0};

  SDL_Texture *screen_texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
      SCREEN_WIDTH, SCREEN_HEIGHT);

  SDL_Event event;

  f64 speed = 0.1;
  f64 rotation_speed = 0.05f;

  bool should_update = true;

  int mouse_x, mouse_y;

  while (game.is_running) {

    SDL_PumpEvents();

    const Uint8 *keystate = SDL_GetKeyboardState(NULL);

    v2 old_player_pos = game.player_pos;

    i32 moving_forward = 0;

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        game.is_running = false;
        break;
      }
    }

    if (keystate[SDL_SCANCODE_W]) {
      game.player_pos.x += cos(game.fov_angle) * speed;
      game.player_pos.y += sin(game.fov_angle) * speed;
      moving_forward = 1;
      should_update = true;
    }

    if (keystate[SDL_SCANCODE_S]) {
      game.player_pos.x -= cos(game.fov_angle) * speed;
      game.player_pos.y -= sin(game.fov_angle) * speed;
      moving_forward = -1;
      should_update = true;
    }

    Uint32 mouse_state = SDL_GetRelativeMouseState(&mouse_x, &mouse_y);

    if (mouse_x != 0) {
      f64 mouse_sensivity = 0.01f;
      game.fov_angle += mouse_x * mouse_sensivity;
      should_update = true;
    }

    SDL_RenderClear(renderer);

    if (should_update) {
      f64 ray = cast_single_ray(game, game.fov_angle);
      f64 back_ray = cast_single_ray(game, game.fov_angle + PI);

      f64 tolerance = 1.15f;
      if (moving_forward == 1 && ray != -1 && ray < tolerance) {
        game.player_pos = old_player_pos;
      }

      if (moving_forward == -1 && back_ray != -1 && back_ray < tolerance) {
        game.player_pos = old_player_pos;
      }

      update_game(game, screen_texture);
    }

    SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    should_update = false;
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
