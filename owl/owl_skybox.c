#include "owl_skybox.h"

#include "owl_internal.h"
#include "owl_vk_context.h"
#include "owl_vk_im_command_buffer.h"
#include "owl_vk_stage_heap.h"
#include "stb_image.h"
#include <stdio.h>

#define OWL_SKYBOX_MAX_PATH_LENGTH 256

owl_private enum owl_code
owl_skybox_image_init (struct owl_skybox           *sb,
                       owl_i32                      w,
                       owl_i32                      h,
                       struct owl_vk_context const *ctx)
{
  VkImageCreateInfo info;

  VkResult vk_result = VK_SUCCESS;

  sb->width  = w;
  sb->height = h;

  info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.pNext         = NULL;
  info.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  info.imageType     = VK_IMAGE_TYPE_2D;
  info.format        = VK_FORMAT_R8G8B8A8_SRGB;
  info.extent.width  = (owl_u32)w;
  info.extent.height = (owl_u32)h;
  info.extent.depth  = 1;
  info.mipLevels     = 1;
  info.arrayLayers   = 6; /* 6 sides of the cube */
  info.samples       = VK_SAMPLE_COUNT_1_BIT;
  info.tiling        = VK_IMAGE_TILING_OPTIMAL;
  info.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
               | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
  info.queueFamilyIndexCount = 0;
  info.pQueueFamilyIndices   = NULL;
  info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

  vk_result = vkCreateImage (ctx->vk_device, &info, NULL, &sb->vk_image);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_skybox_image_deinit (struct owl_skybox           *sb,
                         struct owl_vk_context const *ctx)
{
  vkDestroyImage (ctx->vk_device, sb->vk_image, NULL);
}

owl_private enum owl_code
owl_skybox_memory_init (struct owl_skybox           *sb,
                        struct owl_vk_context const *ctx)
{
  VkMemoryRequirements req;
  VkMemoryAllocateInfo info;

  VkResult      vk_result = VK_SUCCESS;
  enum owl_code code      = OWL_SUCCESS;

  vkGetImageMemoryRequirements (ctx->vk_device, sb->vk_image, &req);

  info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  info.pNext           = NULL;
  info.allocationSize  = req.size;
  info.memoryTypeIndex = owl_vk_context_get_memory_type (
      ctx, req.memoryTypeBits, OWL_MEMORY_PROPERTIES_GPU_ONLY);

  vk_result = vkAllocateMemory (ctx->vk_device, &info, NULL, &sb->vk_memory);
  if (VK_SUCCESS != vk_result)
  {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  vk_result
      = vkBindImageMemory (ctx->vk_device, sb->vk_image, sb->vk_memory, 0);
  if (VK_SUCCESS != vk_result)
  {
    code = OWL_ERROR_UNKNOWN;
    goto error_memory_deinit;
  }

  goto out;

error_memory_deinit:
  vkFreeMemory (ctx->vk_device, sb->vk_memory, NULL);

out:
  return code;
}

owl_private void
owl_skybox_memory_deinit (struct owl_skybox           *sb,
                          struct owl_vk_context const *ctx)
{
  vkFreeMemory (ctx->vk_device, sb->vk_memory, NULL);
}

owl_private enum owl_code
owl_skybox_image_view_init (struct owl_skybox           *sb,
                            struct owl_vk_context const *ctx)
{
  VkImageViewCreateInfo info;

  VkResult vk_result = VK_SUCCESS;

  info.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.pNext                       = NULL;
  info.flags                       = 0;
  info.image                       = sb->vk_image;
  info.viewType                    = VK_IMAGE_VIEW_TYPE_CUBE;
  info.format                      = VK_FORMAT_R8G8B8A8_SRGB;
  info.components.r                = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.g                = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.b                = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.a                = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  info.subresourceRange.baseMipLevel   = 0;
  info.subresourceRange.levelCount     = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount     = 6;

  vk_result
      = vkCreateImageView (ctx->vk_device, &info, NULL, &sb->vk_image_view);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_skybox_image_view_deinit (struct owl_skybox           *sb,
                              struct owl_vk_context const *ctx)
{
  vkDestroyImageView (ctx->vk_device, sb->vk_image_view, NULL);
}

struct owl_skybox_load
{
  owl_i32 width;
  owl_i32 height;
};

owl_private void
owl_skybox_transition (struct owl_skybox const               *sb,
                       struct owl_vk_im_command_buffer const *im,
                       owl_u32                                mips,
                       owl_u32                                layers,
                       VkImageLayout                          src_layout,
                       VkImageLayout                          dst_layout)
{
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags src = VK_PIPELINE_STAGE_NONE_KHR;
  VkPipelineStageFlags dst = VK_PIPELINE_STAGE_NONE_KHR;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask = Defined later */
  /* image_memory_barrier.dstAccessMask = Defined later */
  barrier.oldLayout                       = src_layout;
  barrier.newLayout                       = dst_layout;
  barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
  barrier.image                           = sb->vk_image;
  barrier.subresourceRange.baseMipLevel   = 0;
  barrier.subresourceRange.levelCount     = mips;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount     = layers;

  if (VK_IMAGE_LAYOUT_UNDEFINED == src_layout
      && VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == dst_layout)
  {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
  else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == src_layout
           && VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == dst_layout)
  {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  else if (VK_IMAGE_LAYOUT_UNDEFINED == src_layout
           && VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == dst_layout)
  {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                            | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  }
  else
  {
    owl_assert (0 && "Invalid arguments");
  }

  if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == dst_layout)
  {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == dst_layout)
  {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == dst_layout)
  {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  else
  {
    owl_assert (0 && "Invalid arguments");
  }

  vkCmdPipelineBarrier (im->vk_command_buffer, src, dst, 0, 0, NULL, 0, NULL,
                        1, &barrier);
}

owl_private enum owl_code
owl_skybox_sampler_init (struct owl_skybox           *sb,
                         struct owl_vk_context const *ctx)
{
  VkSamplerCreateInfo info;

  VkResult vk_result;

  info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  info.pNext                   = NULL;
  info.flags                   = 0;
  info.magFilter               = VK_FILTER_LINEAR;
  info.minFilter               = VK_FILTER_LINEAR;
  info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  info.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  info.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  info.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  info.mipLodBias              = 0.0F;
  info.anisotropyEnable        = VK_TRUE;
  info.maxAnisotropy           = 16.0F;
  info.compareEnable           = VK_FALSE;
  info.compareOp               = VK_COMPARE_OP_ALWAYS;
  info.minLod                  = 0.0F;
  info.maxLod                  = VK_LOD_CLAMP_NONE;
  info.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  info.unnormalizedCoordinates = VK_FALSE;

  vk_result = vkCreateSampler (ctx->vk_device, &info, NULL, &sb->vk_sampler);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_skybox_sampler_deinit (struct owl_skybox           *sb,
                           struct owl_vk_context const *ctx)
{
  vkDestroySampler (ctx->vk_device, sb->vk_sampler, NULL);
}

owl_private enum owl_code
owl_skybox_set_init (struct owl_skybox *sb, struct owl_vk_context const *ctx)
{
  VkDescriptorSetAllocateInfo info;

  VkResult vk_result = VK_SUCCESS;

  info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  info.pNext              = NULL;
  info.descriptorPool     = ctx->vk_set_pool;
  info.descriptorSetCount = 1;
  info.pSetLayouts        = &ctx->vk_image_frag_set_layout;

  vk_result = vkAllocateDescriptorSets (ctx->vk_device, &info, &sb->vk_set);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_skybox_set_deinit (struct owl_skybox *sb, struct owl_vk_context const *ctx)
{
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1, &sb->vk_set);
}

owl_private void
owl_skybox_set_write (struct owl_skybox *sb, struct owl_vk_context const *ctx)
{
  VkDescriptorImageInfo descriptors[2];
  VkWriteDescriptorSet  writes[2];

  descriptors[0].sampler     = sb->vk_sampler;
  descriptors[0].imageView   = NULL;
  descriptors[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  descriptors[1].sampler     = VK_NULL_HANDLE;
  descriptors[1].imageView   = sb->vk_image_view;
  descriptors[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  writes[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].pNext            = NULL;
  writes[0].dstSet           = sb->vk_set;
  writes[0].dstBinding       = 0;
  writes[0].dstArrayElement  = 0;
  writes[0].descriptorCount  = 1;
  writes[0].descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[0].pImageInfo       = &descriptors[0];
  writes[0].pBufferInfo      = NULL;
  writes[0].pTexelBufferView = NULL;

  writes[1].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].pNext            = NULL;
  writes[1].dstSet           = sb->vk_set;
  writes[1].dstBinding       = 1;
  writes[1].dstArrayElement  = 0;
  writes[1].descriptorCount  = 1;
  writes[1].descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[1].pImageInfo       = &descriptors[1];
  writes[1].pBufferInfo      = NULL;
  writes[1].pTexelBufferView = NULL;

  vkUpdateDescriptorSets (ctx->vk_device, owl_array_size (writes), writes, 0,
                          NULL);
}

/* NOTE(samuel): this thing is soooo slow, pls fix me */
owl_public enum owl_code
owl_skybox_init (struct owl_skybox           *sb,
                 struct owl_vk_context const *ctx,
                 struct owl_vk_stage_heap    *heap,
                 char const                  *path)
{
  owl_i32 i;
  owl_i32 w;
  owl_i32 h;
  owl_i32 c;

  char src[OWL_SKYBOX_MAX_PATH_LENGTH];

  VkBufferImageCopy copies[6];

  struct owl_vk_im_command_buffer im;
  struct owl_vk_stage_allocation  alloc;

  owl_u64   size;
  owl_u64   offset;
  owl_byte *stage;

  owl_byte     *data = NULL;
  enum owl_code code = OWL_SUCCESS;

  owl_local_persist char const *names[6]
      = { "left.jpg",   "right.jpg", "top.jpg",
          "bottom.jpg", "front.jpg", "back.jpg" };

  snprintf (src, OWL_SKYBOX_MAX_PATH_LENGTH, "%s/%s", path, names[0]);
  data = stbi_load (src, &w, &h, &c, STBI_rgb_alpha);
  if (!data)
  {
    code = OWL_ERROR_NOT_FOUND;
    goto out;
  }

  code = owl_skybox_image_init (sb, w, h, ctx);
  if (OWL_SUCCESS != code)
    goto error_data_free;

  code = owl_skybox_memory_init (sb, ctx);
  if (OWL_SUCCESS != code)
    goto error_image_deinit;

  code = owl_skybox_image_view_init (sb, ctx);
  if (OWL_SUCCESS != code)
    goto error_memory_deinit;

  offset = 0;
  size   = w * h * 4;
  stage  = owl_vk_stage_heap_allocate (heap, ctx, size * 6, &alloc);
  if (!stage)
  {
    code = OWL_ERROR_NO_STAGE_MEMORY;
    goto error_image_view_deinit;
  }

  for (i = 0; i < (owl_i32)owl_array_size (names); ++i)
  {
    /* the first one was preloaded to get the dimensions */
    if (0 < i)
    {
      snprintf (src, OWL_SKYBOX_MAX_PATH_LENGTH, "%s/%s", path, names[i]);
      data = stbi_load (src, &w, &h, &c, STBI_rgb_alpha);
      if (!data)
      {
        code = OWL_ERROR_NOT_FOUND;
        goto error_stage_heap_free;
      }
    }

    copies[i].bufferOffset                    = offset;
    copies[i].bufferRowLength                 = 0;
    copies[i].bufferImageHeight               = 0;
    copies[i].imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    copies[i].imageSubresource.mipLevel       = 0;
    copies[i].imageSubresource.baseArrayLayer = i;
    copies[i].imageSubresource.layerCount     = 1;
    copies[i].imageOffset.x                   = 0;
    copies[i].imageOffset.y                   = 0;
    copies[i].imageOffset.z                   = 0;
    copies[i].imageExtent.width               = (owl_u32)sb->width;
    copies[i].imageExtent.height              = (owl_u32)sb->height;
    copies[i].imageExtent.depth               = 1;

    owl_memcpy (stage + offset, data, size);

    stbi_image_free (data);
    data = NULL;

    offset += size;
  }

  code = owl_vk_im_command_buffer_begin (&im, ctx);
  if (OWL_SUCCESS != code)
    goto error_image_view_deinit;

  owl_skybox_transition (sb, &im, 1, 6, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  vkCmdCopyBufferToImage (im.vk_command_buffer, alloc.vk_buffer, sb->vk_image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, copies);

  owl_skybox_transition (sb, &im, 1, 6, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  /* FIXME(samuel): correctly error out of im command buffer */
  code = owl_vk_im_command_buffer_end (&im, ctx);
  if (OWL_SUCCESS != code)
    goto error_stage_heap_free;

  code = owl_skybox_sampler_init (sb, ctx);
  if (OWL_SUCCESS != code)
    goto error_stage_heap_free;

  code = owl_skybox_set_init (sb, ctx);
  if (OWL_SUCCESS != code)
    goto error_sampler_deinit;

  owl_skybox_set_write (sb, ctx);

  owl_vk_stage_heap_free (heap, ctx);

  goto out;

error_sampler_deinit:
  owl_skybox_sampler_deinit (sb, ctx);

error_stage_heap_free:
  owl_vk_stage_heap_free (heap, ctx);

error_image_view_deinit:
  owl_skybox_image_view_deinit (sb, ctx);

error_memory_deinit:
  owl_skybox_memory_deinit (sb, ctx);

error_image_deinit:
  owl_skybox_image_deinit (sb, ctx);

error_data_free:
  if (data)
    stbi_image_free (data);

out:
  return code;
}

owl_public void
owl_skybox_deinit (struct owl_skybox *sb, struct owl_vk_context const *ctx)
{
  owl_vk_context_device_wait (ctx);

  owl_skybox_set_deinit (sb, ctx);
  owl_skybox_sampler_deinit (sb, ctx);
  owl_skybox_image_view_deinit (sb, ctx);
  owl_skybox_memory_deinit (sb, ctx);
  owl_skybox_image_deinit (sb, ctx);
}
