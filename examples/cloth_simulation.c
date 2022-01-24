#include <owl/owl.h>

#include <stdio.h>

#define CLOTH_H 32
#define CLOTH_W 32
#define PARTICLE_COUNT (CLOTH_W * CLOTH_H)
#define IDXS_H (CLOTH_H - 1)
#define IDXS_W (CLOTH_W - 1)
#define IDXS_COUNT (IDXS_H * IDXS_W * 6)
#define DAMPING 0.002F
#define STEPS 4
#define FRICTION 0.5F
#define GRAVITY 0.05

#define CLOTH_COORD(i, j) ((i)*CLOTH_W + (j))

struct particle {
  int movable;
  owl_v3 pos;
  owl_v3 velocity;
  owl_v3 acceleration;
  owl_v3 previous_position;
  float rest_distances[4];
  struct particle *links[4];
};

struct cloth {
  struct particle particles[PARTICLE_COUNT];

  /* priv draw info */
  struct owl_draw_cmd group_;
  owl_u32 indices_[IDXS_COUNT];
  struct owl_draw_cmd_vertex vertices_[PARTICLE_COUNT];
};

static void init_cloth_(struct cloth *cloth) {
  owl_u32 i, j;

  for (i = 0; i < CLOTH_H; ++i) {
    for (j = 0; j < CLOTH_W; ++j) {
      owl_u32 const k = CLOTH_COORD(i, j);
      float const w = 2.0F * (float)j / (float)(CLOTH_W - 1) - 1.0F;
      float const h = 2.0F * (float)i / (float)(CLOTH_H - 1) - 1.0F;
      struct particle *p = &cloth->particles[k];

      p->movable = 1;

      OWL_SET_V3(w, h, 0.0F, p->pos);
      OWL_SET_V3(0.0F, 0.0F, 0.0F, p->velocity);
      OWL_SET_V3(0.0F, GRAVITY, 0.8F, p->acceleration);
      OWL_SET_V3(w, h, 0.0F, p->previous_position);

      OWL_SET_V3(w, h, 0.0F, cloth->vertices_[k].pos);
      OWL_SET_V3(1.0F, 1.0F, 1.0F, cloth->vertices_[k].color);
      OWL_SET_V2((w + 1.0F) / 2.0F, (h + 1.0F) / 2.0F, cloth->vertices_[k].uv);
    }
  }

  for (i = 0; i < CLOTH_H; ++i) {
    for (j = 0; j < CLOTH_W; ++j) {
      owl_u32 const k = CLOTH_COORD(i, j);
      struct particle *p = &cloth->particles[k];

      if (j) {
        owl_u32 link = CLOTH_COORD(i, j - 1);
        p->links[0] = &cloth->particles[link];
        p->rest_distances[0] = owl_dist_v3(p->pos, p->links[0]->pos);
      } else {
        p->links[0] = NULL;
        p->rest_distances[0] = 0.0F;
      }

      if (i) {
        owl_u32 link = CLOTH_COORD(i - 1, j);
        p->links[1] = &cloth->particles[link];
        p->rest_distances[1] = owl_dist_v3(p->pos, p->links[1]->pos);
      } else {
        p->links[1] = NULL;
        p->rest_distances[1] = 0.0F;
      }

      if (j < CLOTH_H - 1) {
        owl_u32 link = CLOTH_COORD(i, j + 1);
        p->links[2] = &cloth->particles[link];
        p->rest_distances[2] = owl_dist_v3(p->pos, p->links[2]->pos);
      } else {
        p->links[2] = NULL;
        p->rest_distances[2] = 0.0F;
      }

      if (i < CLOTH_W - 1) {
        owl_u32 link = CLOTH_COORD(i + 1, j);
        p->links[3] = &cloth->particles[link];
        p->rest_distances[3] = owl_dist_v3(p->pos, p->links[3]->pos);
      } else {
        p->links[3] = NULL;
        p->rest_distances[3] = 0.0F;
      }
    }
  }

  for (i = 0; i < IDXS_H; ++i) {
    for (j = 0; j < IDXS_W; ++j) {
      owl_u32 const k = i * CLOTH_W + j;
      owl_u32 const fix_k = (i * IDXS_W + j) * 6;

      cloth->indices_[fix_k + 0] = k + CLOTH_W;
      cloth->indices_[fix_k + 1] = k + CLOTH_W + 1;
      cloth->indices_[fix_k + 2] = k + 1;
      cloth->indices_[fix_k + 3] = k + 1;
      cloth->indices_[fix_k + 4] = k;
      cloth->indices_[fix_k + 5] = k + CLOTH_W;
    }
  }

  for (i = 0; i < CLOTH_W; ++i)
    cloth->particles[CLOTH_COORD(0, i)].movable = 0;
}

static void update_cloth(float dt, struct cloth *cloth) {
  owl_u32 i;
  for (i = 0; i < PARTICLE_COUNT; ++i) {
    owl_u32 j;
    owl_v3 tmp;
    struct particle *p = &cloth->particles[i];

    if (!p->movable)
      continue;

    p->acceleration[1] += GRAVITY;

    OWL_SUB_V3(p->pos, p->previous_position, p->velocity);
    OWL_SCALE_V3(p->velocity, 1.0F - DAMPING, p->velocity);
    OWL_SCALE_V3(p->acceleration, dt, tmp);
    OWL_ADD_V3(tmp, p->velocity, tmp);
    OWL_COPY_V3(p->pos, p->previous_position);
    OWL_ADD_V3(p->pos, tmp, p->pos);
    OWL_ZERO_V3(p->acceleration);

    /* constraint */
    for (j = 0; j < STEPS; ++j) {
      owl_u32 k;
      for (k = 0; k < 4; ++k) {
        float factor;
        owl_v3 delta;
        owl_v3 correction;
        struct particle *link = p->links[k];

        if (!link)
          continue;

        OWL_SUB_V3(link->pos, p->pos, delta);
        factor = 1 - (p->rest_distances[k] / owl_mag_v3(delta));
        OWL_SCALE_V3(delta, factor, correction);
        OWL_SCALE_V3(correction, 0.5F, correction);
        OWL_ADD_V3(p->pos, correction, p->pos);

        if (!link->movable)
          continue;

        OWL_NEGATE_V3(correction, correction);
        OWL_ADD_V3(link->pos, correction, link->pos);
      }
    }
  }

  for (i = 0; i < PARTICLE_COUNT; ++i)
    OWL_COPY_V3(cloth->particles[i].pos, cloth->vertices_[i].pos);
}

static void change_particle_position(owl_u32 id, owl_v2 const position,
                                     struct cloth *cloth) {
  struct particle *p = &cloth->particles[id];

  if (p->movable)
    OWL_COPY_V2(position, p->pos);
}

static owl_u32 select_particle_at(owl_v2 const pos, struct cloth *cloth) {
  owl_u32 i;
  owl_u32 current = 0;
  float min_dist = owl_dist_v2(pos, cloth->particles[current].pos);

  for (i = 1; i < PARTICLE_COUNT; ++i) {
    struct particle *p = &cloth->particles[i];
    float const new_dist = owl_dist_v2(pos, p->pos);

    if (new_dist < min_dist) {
      current = i;
      min_dist = new_dist;
    }
  }

  return current;
}

void init_cloth(struct cloth *cloth, struct owl_texture *texture) {
  init_cloth_(cloth);

  cloth->group_.type = OWL_DRAW_CMD_TYPE_BASIC;
  cloth->group_.storage.as_basic.texture = texture;
  cloth->group_.storage.as_basic.indices_count = IDXS_COUNT;
  cloth->group_.storage.as_basic.indices = cloth->indices_;
  cloth->group_.storage.as_basic.vertices_count = PARTICLE_COUNT;
  cloth->group_.storage.as_basic.vertices = cloth->vertices_;
}

struct owl_draw_cmd_ubo *get_cloth_pvm(struct cloth *cloth) {
  return &cloth->group_.storage.as_basic.ubo;
}

char const *fps_string(double time) {
  static char buf[256];
  snprintf(buf, 256, "fps: %.2f\n", 1 / time);
  return buf;
}

#define TEST(fn)                                                               \
  do {                                                                         \
    enum owl_code code = (fn);                                                 \
    if (OWL_SUCCESS != (code)) {                                               \
      printf("something went wrong in call: %s, code %i\n", (#fn), code);      \
      return 0;                                                                \
    }                                                                          \
  } while (0)

static struct owl_window_desc window_desc;
static struct owl_window *window;
static struct owl_vk_renderer *renderer;
static struct owl_texture_desc texture_desc;
static struct owl_texture *texture;
static struct owl_draw_cmd group;
static struct cloth cloth;
static struct owl_font *font;
static struct owl_input_state *input;
static struct owl_text_cmd text;

#define UNSELECTED (owl_u32) - 1
#define TEXPATH "../../assets/Chaeyoung.jpeg"
#define FONTPATH "../../assets/SourceCodePro-Regular.ttf"

int main(void) {
  owl_v3 eye;
  owl_v3 center;
  owl_v3 up;
  owl_v3 position;
  owl_v3 color;
  struct owl_draw_cmd_ubo *pvm;
  owl_u32 selected = UNSELECTED;

  window_desc.height = 600;
  window_desc.width = 600;
  window_desc.title = "cloth-sim";

  TEST(owl_create_window(&window_desc, &input, &window));
  TEST(owl_create_renderer(window, &renderer));

  texture_desc.mip_mode = OWL_SAMPLER_MIP_MODE_LINEAR;
  texture_desc.min_filter = OWL_SAMPLER_FILTER_LINEAR;
  texture_desc.mag_filter = OWL_SAMPLER_FILTER_LINEAR;
  texture_desc.wrap_u = OWL_SAMPLER_ADDR_MODE_REPEAT;
  texture_desc.wrap_v = OWL_SAMPLER_ADDR_MODE_REPEAT;
  texture_desc.wrap_w = OWL_SAMPLER_ADDR_MODE_REPEAT;

  TEST(
      owl_create_texture_from_file(renderer, &texture_desc, TEXPATH, &texture));
  TEST(owl_create_font(renderer, 64, FONTPATH, &font));

  init_cloth(&cloth, texture);

  text.font = font;
  OWL_SET_V3(1.0F, 1.0F, 1.0F, text.color);
  OWL_SET_V2(-1.0F, -0.93F, text.pos);

  pvm = get_cloth_pvm(&cloth);

  OWL_IDENTITY_M4(pvm->model);
  OWL_IDENTITY_M4(pvm->view);
  OWL_IDENTITY_M4(pvm->proj);

#if 0
  owl_ortho_m4(-2.0F, 2.0F, -2.0F, 2.0F, 0.1F, 10.0F, pvm->proj);
#else
  owl_perspective_m4(OWL_DEG_TO_RAD(45.0F), 1.0F, 0.01F, 10.0F, pvm->proj);
#endif

  OWL_SET_V3(0.0F, 0.0F, 2.0F, eye);
  OWL_SET_V3(0.0F, 0.0F, 0.0F, center);
  OWL_SET_V3(0.0F, 1.0F, 0.0F, up);

  owl_look_at_m4(eye, center, up, pvm->view);

  OWL_SET_V3(0.0F, 0.0F, -2.0F, position);

  owl_translate_m4(position, pvm->model);

  while (!owl_is_window_done(window)) {
#if 1

    if (UNSELECTED == selected &&
        OWL_BUTTON_STATE_PRESS == input->mouse[OWL_MOUSE_BUTTON_LEFT])
      selected = select_particle_at(input->cur_cursor_pos, &cloth);

    if (UNSELECTED != selected)
      change_particle_position(selected, input->cur_cursor_pos, &cloth);

    if (UNSELECTED != selected &&
        OWL_BUTTON_STATE_RELEASE == input->mouse[OWL_MOUSE_BUTTON_LEFT])
      selected = UNSELECTED;

#endif

    update_cloth(1.0F / 60.0F, &cloth);

    if (OWL_SUCCESS != owl_begin_frame(renderer)) {
      owl_recreate_swapchain(window, renderer);
      continue;
    }

    owl_bind_pipeline(renderer, OWL_PIPELINE_TYPE_MAIN);
    owl_submit_draw_cmd(renderer, &cloth.group_);

#if 1
    owl_bind_pipeline(renderer, OWL_PIPELINE_TYPE_FONT);
#endif

    text.text = fps_string(input->dt_time);
    owl_submit_text_cmd(renderer, &text);

    if (OWL_SUCCESS != owl_end_frame(renderer)) {
      owl_recreate_swapchain(window, renderer);
      continue;
    }

    owl_poll_window_input(window);
  }

  owl_destroy_font(renderer, font);
  owl_destroy_texture(renderer, texture);
  owl_destroy_renderer(renderer);
  owl_destroy_window(window);
}
