#include "core/metal_ptr.hpp"
#include "assets/glb_asset.hpp"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace quacknado {
namespace {
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

quacknado::metal::Ptr<MTL::Texture> LoadTexture(const cgltf_image& image, MTL::Device* device) {
  Require(image.buffer_view && !image.uri && image.mime_type &&
              (std::strcmp(image.mime_type, "image/png") == 0 ||
               std::strcmp(image.mime_type, "image/jpeg") == 0),
          "Expected embedded GLB PNG or JPEG");
  const auto* bytes = cgltf_buffer_view_data(image.buffer_view);
  Require(bytes && image.buffer_view->size <= std::numeric_limits<int>::max(),
          "Invalid GLB image buffer");
  int width, height, channels;
  std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
      stbi_load_from_memory(bytes, static_cast<int>(image.buffer_view->size),
                            &width, &height, &channels, STBI_rgb_alpha),
      &stbi_image_free);
  if (!pixels) {
    const char* reason = stbi_failure_reason();
    throw std::runtime_error(std::string("GLB image decode failed: ") + (reason ? reason : "unknown error"));
  }
  Require(width > 0 && height > 0, "Invalid GLB image dimensions");
  auto descriptor = quacknado::metal::adopt(MTL::TextureDescriptor::alloc()->init());
  descriptor->setTextureType(MTL::TextureType2D);
  descriptor->setPixelFormat(MTL::PixelFormatRGBA8Unorm_sRGB);
  descriptor->setWidth(width);
  descriptor->setHeight(height);
  descriptor->setMipmapLevelCount(1 + static_cast<NS::UInteger>(std::floor(std::log2(std::max(width, height)))));
  descriptor->setStorageMode(MTL::StorageModeShared);
  descriptor->setUsage(MTL::TextureUsageShaderRead);
  auto texture = quacknado::metal::adopt(device->newTexture(descriptor.get()));
  Require(texture.get(), "GLB texture allocation failed");
  texture->replaceRegion(MTL::Region::Make2D(0, 0, width, height), 0,
                         pixels.get(), static_cast<NS::UInteger>(width) * 4);
  if (texture->mipmapLevelCount() > 1) {
    auto queue = quacknado::metal::adopt(device->newCommandQueue());
    Require(queue.get(), "GLB mipmap command queue allocation failed");
    auto command = quacknado::metal::retain(queue->commandBuffer());
    Require(command.get(), "GLB mipmap command allocation failed");
    auto encoder = quacknado::metal::retain(command->blitCommandEncoder());
    Require(encoder.get(), "GLB mipmap encoder allocation failed");
    encoder->generateMipmaps(texture.get());
    encoder->endEncoding();
    command->commit();
    command->waitUntilCompleted();
    Require(command->status() == MTL::CommandBufferStatusCompleted, "GLB mipmap generation failed");
  }
  return texture;
}
}

GlbAsset LoadGlb(const std::string& path, MTL::Device* device) {
  Require(device, "GLB loading requires a Metal device");
  cgltf_options options {};
  cgltf_data* parsed = nullptr;
  const auto status = cgltf_parse_file(&options, path.c_str(), &parsed);
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(parsed, &cgltf_free);
  Require(status == cgltf_result_success, "Cannot read or parse GLB 2.0 file");
  Require(data->file_type == cgltf_file_type_glb, "Expected GLB file");
  for (size_t i = 0; i < data->extensions_required_count; ++i)
    Require(std::strcmp(data->extensions_required[i], "KHR_materials_pbrSpecularGlossiness") == 0,
            "Unsupported required GLB extension");
  for (size_t i = 0; i < data->buffers_count; ++i)
    Require(!data->buffers[i].uri, "External GLB buffers are unsupported");
  Require(cgltf_load_buffers(&options, data.get(), path.c_str()) == cgltf_result_success,
          "Cannot load GLB binary buffers");
  Require(cgltf_validate(data.get()) == cgltf_result_success, "Invalid GLB accessors or scene data");
  const auto* scene = data->scene ? data->scene : (data->scenes_count ? &data->scenes[0] : nullptr);
  Require(scene, "GLB has no scene");

  GlbAsset result;
  std::unordered_map<const cgltf_image*, quacknado::metal::Ptr<MTL::Texture>> textures;
  simd_float3 minimum {INFINITY, INFINITY, INFINITY};
  simd_float3 maximum {-INFINITY, -INFINITY, -INFINITY};
  std::vector<bool> visiting(data->nodes_count, false);
  std::function<void(const cgltf_node*, simd_float4x4)> visit =
      [&](const cgltf_node* node, simd_float4x4 parent) {
    const size_t node_index = node - data->nodes;
    Require(!visiting[node_index], "Cycle in GLB node hierarchy");
    visiting[node_index] = true;
    Require(!node->skin, "Skinned GLB meshes are unsupported");
    simd_float4x4 local;
    static_assert(sizeof local == sizeof(float) * 16);
    cgltf_float matrix[16];
    cgltf_node_transform_local(node, matrix);
    std::memcpy(&local, matrix, sizeof local);
    const auto world = simd_mul(parent, local);
    if (node->mesh) {
      for (size_t p = 0; p < node->mesh->primitives_count; ++p) {
        const auto& source = node->mesh->primitives[p];
        Require(source.type == cgltf_primitive_type_triangles && !source.has_draco_mesh_compression,
                "Only uncompressed GLB triangle primitives are supported");
        Require(source.targets_count == 0, "GLB morph targets are unsupported");
        const cgltf_accessor* positions = nullptr;
        const cgltf_accessor* uv = nullptr;
        for (size_t a = 0; a < source.attributes_count; ++a) {
          const auto& attribute = source.attributes[a];
          if (attribute.type == cgltf_attribute_type_position) positions = attribute.data;
          if (attribute.type == cgltf_attribute_type_texcoord && attribute.index == 0) uv = attribute.data;
        }
        Require(positions && uv && positions->type == cgltf_type_vec3 && uv->type == cgltf_type_vec2 &&
                    positions->component_type == cgltf_component_type_r_32f && uv->component_type == cgltf_component_type_r_32f &&
                    positions->count == uv->count && positions->count > 0 &&
                    positions->count <= std::numeric_limits<uint32_t>::max() &&
                    !positions->is_sparse && !uv->is_sparse && !positions->normalized && !uv->normalized,
                "Expected matching non-sparse float position and UV accessors");
        ModelPrimitive primitive;
        primitive.vertices.reserve(positions->count);
        for (size_t i = 0; i < positions->count; ++i) {
          float xyz[3], st[2];
          Require(cgltf_accessor_read_float(positions, i, xyz, 3) && cgltf_accessor_read_float(uv, i, st, 2),
                  "Cannot read GLB vertex attributes");
          const auto position = simd_mul(world, simd_float4{xyz[0], xyz[1], xyz[2], 1});
          Require(std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z) &&
                      std::isfinite(st[0]) && std::isfinite(st[1]), "Non-finite GLB vertex attributes");
          minimum = simd_min(minimum, position.xyz);
          maximum = simd_max(maximum, position.xyz);
          primitive.vertices.push_back({position, {st[0], st[1]}});
        }
        if (source.indices) {
          const auto* indices = source.indices;
          Require(indices->type == cgltf_type_scalar && !indices->is_sparse &&
                      (indices->component_type == cgltf_component_type_r_8u ||
                       indices->component_type == cgltf_component_type_r_16u ||
                       indices->component_type == cgltf_component_type_r_32u),
                  "Expected non-sparse unsigned GLB indices");
          primitive.indices.reserve(indices->count);
          for (size_t i = 0; i < indices->count; ++i) {
            const auto index = cgltf_accessor_read_index(indices, i);
            Require(index < positions->count, "GLB vertex index out of range");
            primitive.indices.push_back(static_cast<uint32_t>(index));
          }
        } else {
          for (size_t i = 0; i < positions->count; ++i) primitive.indices.push_back(static_cast<uint32_t>(i));
        }
        Require(!primitive.indices.empty() && primitive.indices.size() % 3 == 0, "Invalid GLB triangle index count");
        const auto* material = source.material;
        Require(material && (material->has_pbr_metallic_roughness || material->has_pbr_specular_glossiness),
                "Expected GLB base color or diffuse material");
        Require(material->alpha_mode != cgltf_alpha_mode_blend, "Blended GLB materials are unsupported");
        if (material->alpha_mode == cgltf_alpha_mode_mask) primitive.alpha_cutoff = material->alpha_cutoff;
        const auto& pbr = material->pbr_metallic_roughness;
        const bool spec_gloss = material->has_pbr_specular_glossiness;
        const auto& diffuse = material->pbr_specular_glossiness;
        std::memcpy(&primitive.base_color, spec_gloss ? diffuse.diffuse_factor : pbr.base_color_factor,
                    sizeof primitive.base_color);
        const auto& map = spec_gloss ? diffuse.diffuse_texture : pbr.base_color_texture;
        Require(map.texture && map.texcoord == 0 && !map.has_transform,
                "Expected base color texture with TEXCOORD_0 and no transform");
        const auto* image = map.texture->image;
        Require(image && !map.texture->has_basisu && !map.texture->has_webp, "Unsupported GLB texture source");
        auto& texture = textures[image];
        if (!texture.get()) texture = LoadTexture(*image, device);
        primitive.texture = texture;
        result.primitives.push_back(std::move(primitive));
      }
    }
    for (size_t i = 0; i < node->children_count; ++i) visit(node->children[i], world);
    visiting[node_index] = false;
  };
  for (size_t i = 0; i < scene->nodes_count; ++i) visit(scene->nodes[i], matrix_identity_float4x4);
  const float height = maximum.y - minimum.y;
  Require(!result.primitives.empty() && height > 0 && std::isfinite(height), "GLB scene has no usable geometry");
  const auto center = (minimum + maximum) * 0.5f;
  for (auto& primitive : result.primitives)
    for (auto& vertex : primitive.vertices) {
      const auto p = (vertex.position.xyz - center) * (2.0f / height);
      vertex.position = {p.x, p.y, p.z, 1};
    }
  return result;
}
}
