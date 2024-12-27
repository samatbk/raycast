#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_rwops.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_surface.h>

#include <SDL2/SDL_timer.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define TEXTURE_WIDTH 16
#define TEXTURE_HEIGHT 16

#define MAP_WIDTH 10
#define MAP_HEIGHT 10

#define PI 3.141592

typedef int8_t i8;
typedef uint8_t u8;

typedef int16_t i16;
typedef uint16_t u16;

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

typedef u32 Map[MAP_HEIGHT][MAP_WIDTH];

typedef enum { EMPTY_PLACE, WALL } map_info;

typedef struct {
  bool is_running;
  u32 pixels[SCREEN_HEIGHT][SCREEN_WIDTH];
  Map map;
} Game;

typedef struct {
  v2 pos;
  f64 dir;
  f64 speed;
  f64 fov;
} Player;

typedef struct {
  bool is_side;
  bool hit;
  f64 distance;
} RayInfo;

void init_sdl() {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "could not initialize sdl2: %s\n", SDL_GetError());
    exit(1);
  }
}

RayInfo cast_ray_dda(v2 pos, f64 angle, Map map) {

  RayInfo ray_info = {.distance = 0.0, .hit = false, .is_side = false};

  f64 tan_angle = (fabs(cos(angle)) > 1e-6) ? tan(angle) : 1e6;
  f64 cot_angle = (fabs(sin(angle)) > 1e-6) ? (1 / tan(angle)) : 1e6;

  v2 unit_step_size =
      (v2){sqrt(1 + tan_angle * tan_angle), sqrt(1 + cot_angle * cot_angle)};

  v2i step = {0, 0};
  v2i map_check = {pos.x, pos.y};

  v2 ray_length = {0, 0};

  if (cos(angle) < 0) {
    step.x = -1;
    ray_length.x = (pos.x - map_check.x) * unit_step_size.x;
  } else {
    step.x = 1;
    ray_length.x = (map_check.x + 1 - pos.x) * unit_step_size.x;
  }

  if (sin(angle) < 0) {
    step.y = -1;
    ray_length.y = (pos.y - map_check.y) * unit_step_size.y;
  } else {
    step.y = 1;
    ray_length.y = (map_check.y + 1 - pos.y) * unit_step_size.y;
  }

  f64 max_distance = 20.0f;
  f64 distance = 0.0f;

  bool tile_found = false;

  while (!tile_found && distance < max_distance) {
    if (ray_length.x < ray_length.y) {
      map_check.x += step.x;
      distance = ray_length.x;
      ray_length.x += unit_step_size.x;
      ray_info.is_side = false;
    } else {
      map_check.y += step.y;
      distance = ray_length.y;
      ray_length.y += unit_step_size.y;
      ray_info.is_side = true;
    }

    v2i map_coords = {(int)floor(map_check.x), (int)floor(map_check.y)};

    if (map_coords.x >= 0 && map_coords.x < MAP_WIDTH && map_coords.y >= 0 &&
        map_coords.y < MAP_HEIGHT) {

      if (map[map_coords.y][map_coords.x] == WALL) {
        tile_found = true;
      }
    }
  }

  if (tile_found) {
    ray_info.distance = distance;
    ray_info.hit = true;
  }

  return ray_info;
}

u32 rgba_color(u8 r, u8 g, u8 b, u8 a) {
  return SDL_MapRGBA(SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888), r, g, b, a);
}

void update_game(Player player, Map map, u32 cool_texture[],
                 SDL_Texture *texture) {
  u32 pixels[SCREEN_HEIGHT][SCREEN_WIDTH] = {0};

  f64 fov = player.fov;
  i32 ray_count = 160;
  f64 angle_step = fov / ray_count;

  u32 floor_color = rgba_color(40, 40, 40, 255);

  for (int j = SCREEN_HEIGHT / 2; j < SCREEN_HEIGHT; j += 1) {
    for (int k = 0; k < SCREEN_WIDTH; k += 1) {

      pixels[j][k] = floor_color;
    }
  }

  for (i32 i = 0; i < ray_count; i++) {
    f64 ray_dir = player.dir - (fov / 2) + i * angle_step;
    RayInfo ray = cast_ray_dda(player.pos, ray_dir, map);
    f64 dist = ray.distance;

    if (!ray.hit) {
      continue;
    }

    i32 wall_width = SCREEN_WIDTH / ray_count;
    i32 wall_height = SCREEN_HEIGHT / dist;

    if (dist < 2.0) {
      dist = 2.0;
      wall_height = SCREEN_HEIGHT / dist;
    }

    u8 opacity = 255 - (dist * 30);
    u32 wall_color = rgba_color(0, opacity, 0, 255);
    u32 wall_color2 = rgba_color(opacity, 0, 0, 255);

    f64 wall_x = 0;
    if (ray.is_side) {
      wall_x = player.pos.x + ray.distance * cos(ray_dir);
    } else {
      wall_x = player.pos.y + ray.distance * sin(ray_dir);
    }

    wall_x -= floor(wall_x);

    u32 tex_x = (u32)(wall_x * (f64)TEXTURE_WIDTH);

    f64 step = 1.0 * TEXTURE_HEIGHT / wall_height;
    f64 tex_pos = 0;

    for (u32 j = SCREEN_HEIGHT / 2 - wall_height;
         j <= SCREEN_HEIGHT / 2 + wall_height; j++) {
      for (u32 k = wall_width * i; k <= wall_width * (i + 1); k++) {

        i32 tex_y = (i32)tex_pos & (TEXTURE_HEIGHT - 1);
        tex_pos += step;

        if (j < SCREEN_HEIGHT) {
          u32 color = cool_texture[TEXTURE_WIDTH * tex_y + tex_x];
          f64 darkness = 1.0 - dist * 0.15;
          if (darkness < 0.1) {
            darkness = 0.1;
          }

          u8 red = ((color >> 24) & 0xFF) * darkness;
          u8 green = ((color >> 16) & 0xFF) * darkness;
          u8 blue = ((color >> 8) & 0xFF) * darkness;

          pixels[j][k] = (red << 24) + (green << 16) + (blue << 8) + 0xFF;
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

  u32 textures[1][TEXTURE_WIDTH * TEXTURE_HEIGHT];

  for (int y = 0; y < TEXTURE_HEIGHT; y++) {
    for (int x = 0; x < TEXTURE_WIDTH; x++) {
      int xycolor = y * 128 / TEXTURE_HEIGHT + x * 128 / TEXTURE_WIDTH;

      textures[0][x + y * TEXTURE_WIDTH] =
          256 * xycolor + 65536 * xycolor + xycolor;
    }
  }

  Player player = (Player){.pos = (v2){.x = 4.0f, .y = 4.0f},
                           .dir = 0,
                           .speed = 0.03f,
                           .fov = PI / 3};

  Game game = (Game){
      .is_running = true,
      .pixels = {0},
      .map = {{1, 1, 1, 1, 1, 1, 1, 0, 0, 0},
              {1, 0, 1, 0, 1, 0, 1, 1, 1, 1},
              {1, 0, 0, 0, 0, 0, 0, 1, 1, 1},
              {1, 0, 0, 0, 0, 0, 0, 0, 1, 1},
              {1, 1, 0, 0, 0, 0, 0, 0, 1, 1},
              {1, 0, 0, 1, 1, 1, 1, 0, 1, 1},
              {1, 0, 0, 0, 0, 0, 1, 0, 1, 0},
              {1, 0, 0, 0, 1, 1, 1, 0, 1, 0},
              {1, 0, 0, 0, 0, 0, 0, 0, 1, 0},
              {1, 1, 1, 1, 1, 1, 1, 1, 1, 0}},
  };

  SDL_Texture *screen_texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
      SCREEN_WIDTH, SCREEN_HEIGHT);

  SDL_Event event;

  f64 rotation_speed = 0.05f;
  bool should_update = true;

  int mouse_x, mouse_y;

  while (game.is_running) {

    SDL_PumpEvents();

    const Uint8 *keystate = SDL_GetKeyboardState(NULL);

    v2 old_player_pos = player.pos;

    i32 moving_forward = 0;

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        game.is_running = false;
        break;
      }
    }

    if (keystate[SDL_SCANCODE_W]) {
      player.pos.x += cos(player.dir) * player.speed;
      player.pos.y += sin(player.dir) * player.speed;
      moving_forward = 1;
      should_update = true;
    }

    if (keystate[SDL_SCANCODE_S]) {
      player.pos.x -= cos(player.dir) * player.speed;
      player.pos.y -= sin(player.dir) * player.speed;
      moving_forward = -1;
      should_update = true;
    }

    u32 mouse_state = SDL_GetRelativeMouseState(&mouse_x, &mouse_y);

    if (mouse_x != 0) {
      f64 mouse_sensivity = 0.01f;
      player.dir += mouse_x * mouse_sensivity;
      should_update = true;
    }

    SDL_RenderClear(renderer);

    if (should_update) {
      RayInfo ray = cast_ray_dda(player.pos, player.dir, game.map);
      RayInfo back_ray = cast_ray_dda(player.pos, player.dir + PI, game.map);

      f64 tolerance = 1.15f;
      if (moving_forward == 1 && ray.hit && ray.distance < tolerance) {
        player.pos = old_player_pos;
      }

      if (moving_forward == -1 && back_ray.hit &&
          back_ray.distance < tolerance) {
        player.pos = old_player_pos;
      }

      update_game(player, game.map, textures[0], screen_texture);
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
