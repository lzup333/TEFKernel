/*******************************************************************************
 * tefkernel - texture2d
 * Copyright (C) 2026 eternalfuture-e38299
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2026/7/25
 *******************************************************************************/

#include "terraria/texture2d.h"

#include "internal/log.h"
#include "patchlib/method.h"
#include "patchlib/property.h"
#include "patchlib/struct/array.h"
#include "terraria/main.h"

static patch_handle_t texture2d_ctor = PATCH_NULL;
static patch_handle_t texture2d_set_data = PATCH_NULL;
static patch_handle_t texture2d_get_height = PATCH_NULL;
static patch_handle_t texture2d_get_width = PATCH_NULL;
static bool server_mode = false;


void terraria_texture2d_init(const bool is_server) {
    server_mode = is_server;
    patch_handle_t texture2d_class = patchlib_type_get_type("Microsoft.Xna.Framework.Graphics", "Texture2D");
    texture2d_ctor = patchlib_type_get_method_by_param_count(texture2d_class, ".ctor", 5);
    texture2d_set_data = patchlib_type_get_method_by_param_count(texture2d_class, "SetData", 1);
    patch_handle_t texture2d_height = patchlib_type_get_property(texture2d_class, "Height");
    patch_handle_t texture2d_width = patchlib_type_get_property(texture2d_class, "Width");

    texture2d_get_height = patchlib_property_get_get_method(texture2d_height);
    texture2d_get_width = patchlib_property_get_get_method(texture2d_width);

    patchlib_free(texture2d_height);
    patchlib_free(texture2d_width);
    patchlib_free(texture2d_class);
}

patch_handle_t terraria_texture2d_create(int width, int height, texture_format_t texture_format, void* data, size_t data_size) {
    TEKLOG_INFO("terraria_texture2d_create: START - %dx%d, format=%d, data_size=%zu",
                width, height, texture_format, data_size);

    if (server_mode) {
        TEKLOG_INFO("terraria_texture2d_create: Server mode, returning NULL");
        return PATCH_NULL;
    }

    static bool mip_map = false;
    patch_handle_t texture2d = PATCH_NULL;

    // 获取 GraphicsDevice
    TEKLOG_INFO("terraria_texture2d_create: Getting GraphicsDevice...");
    patch_handle_t graphics_device = terraria_main_get_graphics_device();
    if (!patchlib_is_valid(graphics_device)) {
        TEKLOG_ERROR("terraria_texture2d_create: Failed to get GraphicsDevice");
        return PATCH_NULL;
    }
    TEKLOG_INFO("terraria_texture2d_create: GraphicsDevice = %p", graphics_device);

    // 检查 texture2d_ctor
    if (!patchlib_is_valid(texture2d_ctor)) {
        TEKLOG_ERROR("terraria_texture2d_create: texture2d_ctor is invalid!");
        patchlib_free(graphics_device);
        return PATCH_NULL;
    }

    // 调用构造函数
    TEKLOG_INFO("terraria_texture2d_create: Calling Texture2D constructor...");
    void* args[5] = { &graphics_device, &width, &height, &mip_map, &texture_format };

    // 打印参数
    TEKLOG_INFO("terraria_texture2d_create: Constructor args:");
    TEKLOG_INFO("  graphics_device: %p", graphics_device);
    TEKLOG_INFO("  width: %d", width);
    TEKLOG_INFO("  height: %d", height);
    TEKLOG_INFO("  mip_map: %d", mip_map);
    TEKLOG_INFO("  texture_format: %d", texture_format);

    bool ctor_result = patchlib_constructor_invoke(texture2d_ctor, &texture2d, args);
    TEKLOG_INFO("terraria_texture2d_create: Constructor result: %d, texture2d=%p",
                ctor_result, texture2d);

    if (!ctor_result || !patchlib_is_valid(texture2d)) {
        TEKLOG_ERROR("terraria_texture2d_create: Failed to create Texture2D! ctor_result=%d, texture2d=%p",
                     ctor_result, texture2d);
        patchlib_free(graphics_device);
        return PATCH_NULL;
    }

    // 验证创建的纹理尺寸
    int tex_width = terraria_texture2d_get_width(texture2d);
    int tex_height = terraria_texture2d_get_height(texture2d);
    TEKLOG_INFO("terraria_texture2d_create: Created Texture2D: %dx%d (expected %dx%d)",
                tex_width, tex_height, width, height);

    // 准备 SetData 泛型方法
    TEKLOG_INFO("terraria_texture2d_create: Getting uint8 type...");
    patch_handle_t uint8_type = patchlib_get_basic_type(PATCH_UINT8);
    if (!patchlib_is_valid(uint8_type)) {
        TEKLOG_ERROR("terraria_texture2d_create: Failed to get uint8 type");
        patchlib_free(graphics_device);
        patchlib_free(texture2d);
        return PATCH_NULL;
    }
    TEKLOG_INFO("terraria_texture2d_create: uint8_type = %p", uint8_type);

    tefstd_vector_t generic_type;
    tefstd_vector_init(&generic_type, sizeof(patch_handle_t));
    tefstd_vector_push_back(&generic_type, &uint8_type);

    TEKLOG_INFO("terraria_texture2d_create: Creating generic instance of SetData...");
    patch_handle_t set_data_uint8 = patchlib_method_make_generic_instance(texture2d_set_data, &generic_type);
    if (!patchlib_is_valid(set_data_uint8)) {
        TEKLOG_ERROR("terraria_texture2d_create: Failed to create generic SetData method!");
        patchlib_free(graphics_device);
        patchlib_free(uint8_type);
        patchlib_free(texture2d);
        tefstd_vector_destroy(&generic_type);
        return PATCH_NULL;
    }
    TEKLOG_INFO("terraria_texture2d_create: set_data_uint8 = %p", set_data_uint8);

    // 创建数组并复制数据
    TEKLOG_INFO("terraria_texture2d_create: Creating array of size %zu...", data_size);
    patch_handle_t data_array = patchlib_array_create(data_size, uint8_type);
    if (!patchlib_is_valid(data_array)) {
        TEKLOG_ERROR("terraria_texture2d_create: Failed to create data array!");
        patchlib_free(graphics_device);
        patchlib_free(uint8_type);
        patchlib_free(texture2d);
        patchlib_free(set_data_uint8);
        tefstd_vector_destroy(&generic_type);
        return PATCH_NULL;
    }
    TEKLOG_INFO("terraria_texture2d_create: data_array = %p", data_array);

    TEKLOG_INFO("terraria_texture2d_create: Copying data to array...");
    bool copy_result = patchlib_array_copy_from_c(data_array, data, data_size);
    TEKLOG_INFO("terraria_texture2d_create: Copy result: %d", copy_result);

    // 调用 SetData
    TEKLOG_INFO("terraria_texture2d_create: Calling SetData...");
    void* set_data_args[1] = { &data_array };
    bool set_result = patchlib_method_invoke_args(set_data_uint8, texture2d, NULL, set_data_args);
    TEKLOG_INFO("terraria_texture2d_create: SetData result: %d", set_result);

    // 清理资源
    TEKLOG_INFO("terraria_texture2d_create: Cleaning up...");
    patchlib_free(graphics_device);
    patchlib_free(uint8_type);
    patchlib_free(data_array);
    patchlib_free(set_data_uint8);
    tefstd_vector_destroy(&generic_type);

    TEKLOG_INFO("terraria_texture2d_create: SUCCESS - returning %p", texture2d);
    return texture2d;
}

int terraria_texture2d_get_width(patch_handle_t texture2d) {
    int width = -1;
    patchlib_method_invoke_args(texture2d_get_width, texture2d, &width, NULL);
    return width;
}

int terraria_texture2d_get_height(patch_handle_t texture2d) {
    int height = -1;
    patchlib_method_invoke_args(texture2d_get_height, texture2d, &height, NULL);
    return height;
}

patch_handle_t terraria_texture2d_get_class() {
    return patchlib_type_get_type("Microsoft.Xna.Framework.Graphics", "Texture2D");
}