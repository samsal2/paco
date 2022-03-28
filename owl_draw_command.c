#include "owl_draw_command.h"

#include "owl_camera.h"
#include "owl_font.h"
#include "owl_internal.h"
#include "owl_model.h"
#include "owl_renderer.h"
#include "owl_vector_math.h"

enum owl_code
owl_draw_command_submit_basic(struct owl_renderer *renderer,
                              struct owl_camera const *camera,
                              struct owl_draw_command_basic const *command) {
  owl_u64 size;
  VkDescriptorSet sets[2];
  struct owl_draw_command_uniform uniform;
  struct owl_renderer_dynamic_heap_reference vertex_reference;
  struct owl_renderer_dynamic_heap_reference index_reference;
  struct owl_renderer_dynamic_heap_reference uniform_reference;
  enum owl_code code = OWL_SUCCESS;

  size = (owl_u64)command->vertices_count * sizeof(*command->vertices);
  code = owl_renderer_dynamic_heap_submit(renderer, size, command->vertices,
                                          &vertex_reference);

  if (OWL_SUCCESS != code)
    goto end;

  size = (owl_u64)command->indices_count * sizeof(*command->indices);
  code = owl_renderer_dynamic_heap_submit(renderer, size, command->indices,
                                          &index_reference);

  if (OWL_SUCCESS != code)
    goto end;

  OWL_M4_COPY(camera->projection, uniform.projection);
  OWL_M4_COPY(camera->view, uniform.view);
  OWL_M4_COPY(command->model, uniform.model);

  size = sizeof(uniform);
  code = owl_renderer_dynamic_heap_submit(renderer, size, &uniform,
                                          &uniform_reference);

  if (OWL_SUCCESS != code)
    goto end;

  sets[0] = uniform_reference.common_ubo_set;
  sets[1] = renderer->image_manager_sets[command->image.slot];

  vkCmdBindVertexBuffers(renderer->active_frame_command_buffer, 0, 1,
                         &vertex_reference.buffer, &vertex_reference.offset);

  vkCmdBindIndexBuffer(renderer->active_frame_command_buffer,
                       index_reference.buffer, index_reference.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(
      renderer->active_frame_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      renderer->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets), sets, 1,
      &uniform_reference.offset32);

  vkCmdDrawIndexed(renderer->active_frame_command_buffer,
                   command->indices_count, 1, 0, 0, 0);

end:
  return code;
}

enum owl_code
owl_draw_command_submit_quad(struct owl_renderer *renderer,
                             struct owl_camera const *camera,
                             struct owl_draw_command_quad const *command) {
  owl_u64 size;
  VkDescriptorSet sets[2];
  struct owl_draw_command_uniform uniform;
  struct owl_renderer_dynamic_heap_reference vertex_reference;
  struct owl_renderer_dynamic_heap_reference index_reference;
  struct owl_renderer_dynamic_heap_reference uniform_reference;
  enum owl_code code = OWL_SUCCESS;
  owl_u32 const indices[] = {2, 3, 1, 1, 0, 2};

  size = OWL_ARRAY_SIZE(command->vertices) * sizeof(command->vertices[0]);
  code = owl_renderer_dynamic_heap_submit(renderer, size, command->vertices,
                                          &vertex_reference);

  if (OWL_SUCCESS != code)
    goto end;

  size = OWL_ARRAY_SIZE(indices) * sizeof(indices[0]);
  code = owl_renderer_dynamic_heap_submit(renderer, size, indices,
                                          &index_reference);

  if (OWL_SUCCESS != code)
    goto end;

  OWL_M4_COPY(camera->projection, uniform.projection);
  OWL_M4_COPY(camera->view, uniform.view);
  OWL_M4_COPY(command->model, uniform.model);

  size = sizeof(uniform);
  code = owl_renderer_dynamic_heap_submit(renderer, size, &uniform,
                                          &uniform_reference);

  if (OWL_SUCCESS != code)
    goto end;

  sets[0] = uniform_reference.common_ubo_set;
  sets[1] = renderer->image_manager_sets[command->image.slot];

  vkCmdBindVertexBuffers(renderer->active_frame_command_buffer, 0, 1,
                         &vertex_reference.buffer, &vertex_reference.offset);

  vkCmdBindIndexBuffer(renderer->active_frame_command_buffer,
                       index_reference.buffer, index_reference.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(
      renderer->active_frame_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      renderer->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets), sets, 1,
      &uniform_reference.offset32);

  vkCmdDrawIndexed(renderer->active_frame_command_buffer,
                   OWL_ARRAY_SIZE(indices), 1, 0, 0, 0);
end:
  return code;
}

enum owl_code
owl_draw_command_submit_text(struct owl_renderer *renderer,
                             struct owl_camera const *camera,
                             struct owl_draw_command_text const *command) {
  char const *l;
  owl_v2 offset;
  enum owl_code code = OWL_SUCCESS;

  OWL_V2_ZERO(offset);

  for (l = command->text; '\0' != *l; ++l) {
    struct owl_font_glyph glyph;
    struct owl_draw_command_quad quad;

    owl_font_fill_glyph(command->font, *l, offset, &glyph);


    quad.image.slot = command->font->atlas.slot;
    OWL_M4_IDENTITY(quad.model);

    OWL_V3_COPY(glyph.positions[0], quad.vertices[0].position);
    OWL_V3_COPY(command->color, quad.vertices[0].color);
    OWL_V2_COPY(glyph.uvs[0], quad.vertices[0].uv);

    OWL_V3_COPY(glyph.positions[1], quad.vertices[1].position);
    OWL_V3_COPY(command->color, quad.vertices[1].color);
    OWL_V2_COPY(glyph.uvs[1], quad.vertices[1].uv);

    OWL_V3_COPY(glyph.positions[2], quad.vertices[2].position);
    OWL_V3_COPY(command->color, quad.vertices[2].color);
    OWL_V2_COPY(glyph.uvs[2], quad.vertices[2].uv);

    OWL_V3_COPY(glyph.positions[3], quad.vertices[3].position);
    OWL_V3_COPY(command->color, quad.vertices[3].color);
    OWL_V2_COPY(glyph.uvs[3], quad.vertices[3].uv);

    if (OWL_SUCCESS != code)
      goto end;

    owl_draw_command_submit_quad(renderer, camera, &quad);
  }

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_model_submit_node_(struct owl_renderer *renderer,
                       struct owl_camera const *camera,
                       struct owl_draw_command_model const *command,
                       struct owl_model_node const *node) {

  owl_i32 i;
  struct owl_model_node parent;
  struct owl_model const *model;
  struct owl_model_uniform uniform;
  struct owl_model_uniform_params uniform_params;
  struct owl_model_node_data const *node_data;
  struct owl_model_mesh_data const *mesh_data;
  struct owl_model_skin_data const *skin_data;
  struct owl_model_skin_ssbo_data *ssbo;
  /* TODO(samuel): materials */
#if 0
  struct owl_model_push_constant push_constant;
#endif
  enum owl_code code = OWL_SUCCESS;

  model = command->skin;
  node_data = &model->nodes[node->slot];

  for (i = 0; i < node_data->children_count; ++i) {
    code = owl_model_submit_node_(renderer, camera, command,
                                  &node_data->children[i]);

    if (OWL_SUCCESS != code)
      goto end;
  }

  if (OWL_MODEL_NODE_NO_MESH_SLOT == node_data->mesh.slot)
    goto end;

  mesh_data = &model->meshes[node_data->mesh.slot];

  if (!mesh_data->primitives_count)
    goto end;

  skin_data = &model->skins[node_data->skin.slot];
  ssbo = skin_data->ssbo_datas[renderer->active_frame_index];

  OWL_M4_COPY(model->nodes[node->slot].matrix, ssbo->matrix);

  for (parent.slot = model->nodes[node->slot].parent.slot;
       OWL_MODEL_NODE_NO_PARENT_SLOT != parent.slot;
       parent.slot = model->nodes[parent.slot].parent.slot)
    owl_m4_multiply(model->nodes[parent.slot].matrix, ssbo->matrix,
                    ssbo->matrix);

  OWL_M4_COPY(command->model, uniform.model);
  OWL_V4_ZERO(uniform_params.light_direction);

  /* TODO(samuel): materials */
#if 0
  vkCmdPushConstants(
      renderer->active_frame_command_buffer, renderer->active_pipeline_layout,
      VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constant), &push_constant);
#endif

  for (i = 0; i < mesh_data->primitives_count; ++i) {
    owl_u32 offsets[2];
    VkDescriptorSet sets[5];
    struct owl_model_primitive primitive;
    struct owl_renderer_dynamic_heap_reference uniform_reference;
    struct owl_renderer_dynamic_heap_reference uniform_params_reference;
    struct owl_model_material material;
    struct owl_model_texture base_color_texture;
    struct owl_model_texture normal_texture;
    struct owl_model_image base_color_image;
    struct owl_model_image normal_image;
    struct owl_model_primitive_data const *primitive_data;
    struct owl_model_material_data const *material_data;
    struct owl_model_texture_data const *base_color_texture_data;
    struct owl_model_texture_data const *normal_texture_data;
    struct owl_model_image_data const *base_color_image_data;
    struct owl_model_image_data const *normal_image_data;

    primitive.slot = mesh_data->primitives[i].slot;
    primitive_data = &model->primitives[primitive.slot];

    if (!primitive_data->count)
      continue;

    material.slot = primitive_data->material.slot;
    material_data = &model->materials[material.slot];

    base_color_texture.slot = material_data->base_color_texture.slot;
    base_color_texture_data = &model->textures[base_color_texture.slot];

    normal_texture.slot = material_data->normal_texture.slot;
    normal_texture_data = &model->textures[normal_texture.slot];

    base_color_image.slot = base_color_texture_data->image.slot;
    base_color_image_data = &model->images[base_color_image.slot];

    normal_image.slot = normal_texture_data->image.slot;
    normal_image_data = &model->images[normal_image.slot];

    OWL_M4_COPY(camera->projection, uniform.projection);
    OWL_M4_COPY(camera->view, uniform.view);
    OWL_V4_ZERO(uniform.light);
    OWL_V3_COPY(command->light, uniform.light);

    code = owl_renderer_dynamic_heap_submit(renderer, sizeof(uniform), &uniform,
                                            &uniform_reference);

    if (OWL_SUCCESS != code)
      goto end;

    code = owl_renderer_dynamic_heap_submit(
        renderer, sizeof(uniform_params), &uniform, &uniform_params_reference);

    if (OWL_SUCCESS != code)
      goto end;

    /* TODO(samuel): generic pipeline layout ordered by frequency */
    sets[0] = uniform_reference.model_ubo_set;
    sets[1] = renderer->image_manager_sets[base_color_image_data->image.slot];
    sets[2] = renderer->image_manager_sets[normal_image_data->image.slot];
    sets[3] = skin_data->ssbo_sets[renderer->active_frame_index];
    sets[4] = uniform_params_reference.model_ubo_params_set;

    offsets[0] = uniform_reference.offset32;
    offsets[1] = uniform_params_reference.offset32;

    vkCmdBindDescriptorSets(
        renderer->active_frame_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        renderer->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets), sets,
        OWL_ARRAY_SIZE(offsets), offsets);

    vkCmdDrawIndexed(renderer->active_frame_command_buffer,
                     primitive_data->count, 1, primitive_data->first, 0, 0);
  }

end:
  return code;
}

enum owl_code
owl_draw_command_submit_model(struct owl_renderer *renderer,
                              struct owl_camera const *camera,
                              struct owl_draw_command_model const *command) {
  owl_i32 i;
  owl_u64 offset = 0;
  enum owl_code code = OWL_SUCCESS;
  struct owl_model const *model = command->skin;

  vkCmdBindVertexBuffers(renderer->active_frame_command_buffer, 0, 1,
                         &model->vertices_buffer, &offset);

  vkCmdBindIndexBuffer(renderer->active_frame_command_buffer,
                       model->indices_buffer, 0, VK_INDEX_TYPE_UINT32);

  for (i = 0; i < command->skin->roots_count; ++i) {
    code = owl_model_submit_node_(renderer, camera, command, &model->roots[i]);

    if (OWL_SUCCESS != code)
      goto end;
  }

end:
  return code;
}

enum owl_code
owl_draw_command_submit_grid(struct owl_renderer *renderer,
                             struct owl_camera const *camera,
                             struct owl_draw_command_grid const *command) {
  struct owl_draw_command_uniform uniform;
  struct owl_renderer_dynamic_heap_reference uniform_reference;
  enum owl_code code = OWL_SUCCESS;

  OWL_M4_COPY(camera->projection, uniform.projection);
  OWL_M4_COPY(camera->view, uniform.view);
  OWL_M4_IDENTITY(uniform.model); /* UNUSED */

  owl_renderer_dynamic_heap_submit(renderer, sizeof(uniform), &uniform,
                                   &uniform_reference);

  vkCmdBindDescriptorSets(
      renderer->active_frame_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      renderer->active_pipeline_layout, 0, 1, &uniform_reference.common_ubo_set,
      1, &uniform_reference.offset32);

  OWL_UNUSED(command);

  vkCmdDraw(renderer->active_frame_command_buffer, 12, 1, 0, 0);

  return code;
}
