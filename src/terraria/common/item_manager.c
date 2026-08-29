/*******************************************************************************
 * tefkernel - item_manager
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
 * Created: 2026/8/26
 *******************************************************************************/

#include "internal/terraria/item_manager.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "internal/log.h"
#include "../../patchlib/il2cpp_api.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#include "terraria/asset.h"
#include "terraria/texture2d.h"

tefstd_vector_t g_terraria_item_registry; // 存储 terraria_item_handle_t*
static bool g_ids_assigned = false;

static patch_handle_t item_id_count;

// 未知物品的属性设置
static void unknown_item_set_defaults(terraria_item_handle_t* current, patch_handle_t instance) {}

// 生成未知物品纹理
static patch_handle_t unknown_item_get_texture(terraria_item_handle_t* current) {
    const int SIZE = 64;
    const size_t PIXEL_COUNT = SIZE * SIZE;
    const size_t DATA_SIZE = PIXEL_COUNT * 4; // RGBA32

    // 分配像素数据
    uint8_t* pixels = malloc(DATA_SIZE);
    if (!pixels) {
        return 0; // 内存分配失败
    }

    // 定义颜色
    const uint8_t COLOR1[4] = {103, 80, 164, 255};  // 深紫
    const uint8_t COLOR2[4] = {0,   0,   0,   255}; // 纯黑
    const uint8_t COLOR3[4] = {103, 80, 164, 255}; // 浅紫（同COLOR1）
    const uint8_t COLOR4[4] = {103, 80, 164, 255}; // 浅紫（同COLOR1）
    const uint8_t DIVIDER_COLOR[4] = {50, 50, 50, 255};

    const int CELL_SIZE = SIZE / 2;

    // 填充像素
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            const size_t index = (y * SIZE + x) * 4;

            // 判断是否在分隔线上
            const bool onDividerX = (x == CELL_SIZE);
            const bool onDividerY = (y == CELL_SIZE);

            if (onDividerX || onDividerY) {
                // 绘制分隔线
                memcpy(&pixels[index], DIVIDER_COLOR, 4);
                continue;
            }

            // 决定当前像素属于哪个格子
            const bool isLeft = (x < CELL_SIZE);
            const bool isTop = (y < CELL_SIZE);

            // 分配颜色（交错布局）
            if (isTop && isLeft) {
                memcpy(&pixels[index], COLOR1, 4); // 左上：深紫
            } else if (isTop && !isLeft) {
                memcpy(&pixels[index], COLOR2, 4); // 右上：纯黑
            } else if (!isTop && isLeft) {
                memcpy(&pixels[index], COLOR3, 4); // 左下：浅紫
            } else {
                memcpy(&pixels[index], COLOR4, 4); // 右下：浅紫
            }
        }
    }

    // 创建纹理
    patch_handle_t texture = terraria_texture2d_create(
        SIZE,
        SIZE,
        TEXTURE_FORMAT_RGBA32,
        pixels,
        DATA_SIZE
    );

    // 释放像素数据（纹理创建时会拷贝数据）
    free(pixels);

    patch_handle_t texture_type = terraria_texture2d_get_class();
    patch_handle_t asset = terraria_asset_create(texture_type, texture);

    patchlib_free(texture_type);
    patchlib_free(texture);

    return asset;
}

// 创建未知物品实例
static terraria_item_handle_t unknown_item = {
    .parent = "Terraria",
    .internal_name = "Unknown",
    .runtime_id = -1,
    .item_ops = {
        .init_static = NULL,
        .set_defaults = unknown_item_set_defaults,
        .get_texture = unknown_item_get_texture
    }
};

static patch_handle_t f_type = PATCH_NULL;

static patch_handle_t f_texture_item = PATCH_NULL;
static patch_handle_t f_texture_item_flame = PATCH_NULL;


static void set_defaults_postfix(patch_handle_t this, void **args, void *result,
                               const patch_method_signature_t *sig_info) {

    // 检查 args 是否有效
    if (!args || !args[0] || !this) {
        TEKLOG_DEBUG("set_defaults_postfix: args[0] is NULL");
        return;
    }

    // 安全读取 args_type
    int args_type = 0;
    // 尝试读取为 int
    args_type = *(int*)args[0];

    TEKLOG_DEBUG("set_defaults_postfix: raw args_type=%d", args_type);

    int base_count = terraria_item_id_get_count();
    size_t custom_count = tefstd_vector_size(&g_terraria_item_registry);
    int total_count = base_count + (int)custom_count;

    // 检查 args_type 是否在有效范围内（包括负数）
    if (args_type <= 0 || args_type > total_count) {
        TEKLOG_DEBUG("set_defaults_postfix: args_type=%d out of range [1, %d], skipping", args_type, total_count);
        return;
    }

    // 设置 Item.type
    patchlib_field_set_value(f_type, this, args[0]);

    const int nid = args_type - base_count;
    TEKLOG_DEBUG("set_defaults_postfix: nid=%d (custom item index)", nid);

    if (nid >= 0 && nid < (int)custom_count) {
        terraria_item_handle_t** item_handle_ptr = tefstd_vector_at(&g_terraria_item_registry, nid);
        if (item_handle_ptr && *item_handle_ptr) {
            terraria_item_handle_t* item_handle = *item_handle_ptr;
            if (item_handle->item_ops.set_defaults) {
                TEKLOG_DEBUG("set_defaults_postfix: calling set_defaults for %s.%s (id=%d)",
                             item_handle->parent, item_handle->internal_name, args_type);
                item_handle->item_ops.set_defaults(item_handle, this);
            }
        }
    } else {
        // 这是原版物品，不需要处理
        TEKLOG_DEBUG("set_defaults_postfix: item %d is vanilla, skipping", args_type);
    }
}

// 比较函数（用于 qsort）
static int compare_item(const void *a, const void *b) {
    const terraria_item_handle_t *item_a = *(const terraria_item_handle_t **) a;
    const terraria_item_handle_t *item_b = *(const terraria_item_handle_t **) b;

    // 先按 modloader 字典序
    const int cmp = strcmp(item_a->parent, item_b->parent);
    if (cmp != 0) return cmp;

    // 同一 modloader 内按 internal_name 字典序
    return strcmp(item_a->internal_name, item_b->internal_name);
}

// 初始化

#if !__ANDROID__
// PC端：AssetInitializer.LoadTextures 会按 TextureAssets.Item 的新长度遍历，
// 并读取 ItemID.Sets.TextureCopyLoad[i]（原版长度只有 ItemID.Count），
// 导致 IndexOutOfRangeException 中断整个贴图加载（表现为游戏无贴图）。
// 同时该函数会用原版 Images/Item_x 覆盖自定义物品贴图。
// 因此：1) resize 时同步扩充 TextureCopyLoad；2) LoadTextures 之后重新写入自定义贴图。
static void load_textures_postfix(patch_handle_t this, void **args, void *result,
                                  const patch_method_signature_t *sig_info) {
    TEKLOG_INFO("AssetInitializer.LoadTextures postfix: re-applying custom item textures");
    terraria_item_manager_init_texture2d();
}

static void resize_array(const char* ns, const char* cls, const char* fid,
                         patch_handle_t type, const int nsize);

// 一次性扩容 ItemID.Sets 中所有长度为 ItemID.Count 的静态数组。
// 游戏代码大量按 item.type 索引这些集合（ItemIconPulse、TrapSigned、TextureCopyLoad 等），
// 任何一个没扩容都会导致绘制自定义物品时 IndexOutOfRangeException（表现为图标"透明"或崩溃）。
static void resize_all_itemid_sets(const int base_count, const int new_size) {
    patch_handle_t sets_class = patchlib_type_get_type("Terraria.ID", "ItemID+Sets");
    if (!patchlib_is_valid(sets_class)) {
        TEKLOG_ERROR("resize_all_itemid_sets: Failed to get ItemID+Sets class");
        return;
    }

    int size = 0;
    void** fields = (void**) il2cpp_class_get_fields(sets_class, &size);
    if (!fields || size <= 0) {
        TEKLOG_ERROR("resize_all_itemid_sets: Failed to get fields of ItemID+Sets");
        patchlib_free(sets_class);
        return;
    }

    int resized = 0;
    int skipped = 0;
    for (int i = 0; i < size; i++) {
        patch_handle_t field = fields[i];
        if (!patchlib_is_valid(field)) continue;

        patch_handle_t array = PATCH_NULL;
        patchlib_field_get_value(field, NULL, &array);
        if (!patchlib_is_valid(array)) {
            continue;
        }

        const size_t len = patchlib_array_length(array);
        if (len != (size_t)base_count) {
            patchlib_free(array);
            skipped++;
            continue;
        }

        patch_handle_t new_array = il2cpp_array_resize(array, new_size);
        if (patchlib_is_valid(new_array)) {
            patchlib_field_set_value(field, NULL, &new_array);
            const char* name = patchlib_field_get_name(field);
            TEKLOG_DEBUG("resize_all_itemid_sets: resized ItemID.Sets.%s (%zu -> %d)",
                         name ? name : "?", len, new_size);
            patchlib_free(new_array);
            resized++;
        } else {
            skipped++;
        }
        patchlib_free(array);
    }

    TEKLOG_INFO("resize_all_itemid_sets: %d arrays resized, %d skipped", resized, skipped);
    patchlib_free(sets_class);
}

static void resize_texture_copy_load(const int new_size) {
    patch_handle_t sets_class = patchlib_type_get_type("Terraria.ID", "ItemID+Sets");
    if (!patchlib_is_valid(sets_class)) {
        TEKLOG_ERROR("resize_texture_copy_load: Failed to get ItemID+Sets class");
        return;
    }

    patch_handle_t field = patchlib_type_get_field(sets_class, "TextureCopyLoad");
    if (!patchlib_is_valid(field)) {
        TEKLOG_ERROR("resize_texture_copy_load: Failed to get TextureCopyLoad field");
        patchlib_free(sets_class);
        return;
    }

    patch_handle_t int32_type = patchlib_get_basic_type(PATCH_INT32);
    resize_array("Terraria.ID", "ItemID+Sets", "TextureCopyLoad", int32_type, new_size);

    // 将新增区域填充为 -1（表示无复制源，与原版默认值一致）
    patch_handle_t array = PATCH_NULL;
    patchlib_field_get_value(field, NULL, &array);
    if (patchlib_is_valid(array)) {
        const int base_count = terraria_item_id_get_count();
        for (int i = base_count; i < new_size; i++) {
            const int32_t value = -1;
            patchlib_array_set(array, (size_t)i, &value);
        }
        patchlib_free(array);
    }

    patchlib_free(int32_type);
    patchlib_free(field);
    patchlib_free(sets_class);
}

// Main.itemAnimations 是 DrawAnimation[ItemID.Count]，
// 绘制自定义物品图标（ItemSlot.DrawItemIcon / Main.LoadItem / 更新动画等）时会按 type 索引，必须同步扩容。
// 新增区域为 null，表示自定义物品没有动画帧，符合预期。
static void resize_item_animations(const int new_size) {
    patch_handle_t draw_animation_class = patchlib_type_get_type("Terraria.DataStructures", "DrawAnimation");
    if (!patchlib_is_valid(draw_animation_class)) {
        TEKLOG_ERROR("resize_item_animations: Failed to get DrawAnimation class");
        return;
    }

    patch_handle_t main_class = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(main_class)) {
        TEKLOG_ERROR("resize_item_animations: Failed to get Main class");
        patchlib_free(draw_animation_class);
        return;
    }

    patch_handle_t field = patchlib_type_get_field(main_class, "itemAnimations");
    if (patchlib_is_valid(field)) {
        resize_array("Terraria", "Main", "itemAnimations", draw_animation_class, new_size);
        patchlib_free(field);
    } else {
        TEKLOG_ERROR("resize_item_animations: Failed to get Main.itemAnimations field");
    }

    patchlib_free(main_class);
    patchlib_free(draw_animation_class);
}
#endif

void terraria_item_manager_init() {
    if (!tefstd_vector_init(&g_terraria_item_registry, sizeof(terraria_item_handle_t *))) return;

    terraria_item_manager_register_item(&unknown_item);

    patch_handle_t item_id_class = patchlib_type_get_type("Terraria.ID", "ItemID");
    item_id_count = patchlib_type_get_field(item_id_class, "Count");

    patch_handle_t item_class = patchlib_type_get_type("Terraria", "Item");
    f_type = patchlib_type_get_field(item_class, "type");
    patch_handle_t set_defaults = patchlib_type_get_method_by_param_count(item_class, "SetDefaults", 2);

    patchlib_install_prepost_hook(set_defaults, NULL, set_defaults_postfix);

    patch_handle_t texture_asset_class = patchlib_type_get_type("Terraria.GameContent", "TextureAssets");
    f_texture_item = patchlib_type_get_field(texture_asset_class, "Item");
    f_texture_item_flame = patchlib_type_get_field(texture_asset_class, "ItemFlame");

    patchlib_free(texture_asset_class);
    patchlib_free(item_class);
    patchlib_free(set_defaults);
    patchlib_free(item_id_class);

#if !__ANDROID__
    // PC端：在原版贴图加载完成后重新写入自定义物品贴图（原版会覆盖整个数组）
    patch_handle_t asset_initializer_class = patchlib_type_get_type("Terraria.Initializers", "AssetInitializer");
    if (patchlib_is_valid(asset_initializer_class)) {
        patch_handle_t load_textures = patchlib_type_get_method_by_param_count(asset_initializer_class, "LoadTextures", 1);
        if (patchlib_is_valid(load_textures)) {
            patch_hook_id_t hook_id = patchlib_install_prepost_hook(load_textures, NULL, load_textures_postfix);
            if (hook_id != PATCH_HOOK_INVALID_ID) {
                TEKLOG_INFO("AssetInitializer.LoadTextures hook installed successfully");
            } else {
                TEKLOG_ERROR("Failed to install AssetInitializer.LoadTextures hook");
            }
            patchlib_free(load_textures);
        } else {
            TEKLOG_ERROR("Failed to get AssetInitializer.LoadTextures method");
        }
        patchlib_free(asset_initializer_class);
    } else {
        TEKLOG_ERROR("Failed to get AssetInitializer class");
    }
#endif
}

void terraria_item_manager_destroy() {
    // 注意：只释放 vector，不释放 item 本身（由调用者管理）
    tefstd_vector_destroy(&g_terraria_item_registry);
    g_ids_assigned = false;
}

bool terraria_item_manager_register_item(terraria_item_handle_t* item_handle) {
    if (!item_handle || !item_handle->parent || !item_handle->internal_name)
        return false;

    // 检查是否已存在
    const size_t count = tefstd_vector_size(&g_terraria_item_registry);
    for (size_t i = 0; i < count; i++) {
        terraria_item_handle_t** existing = tefstd_vector_at(&g_terraria_item_registry, i);
        if (!existing || !*existing) continue;

        if (strcmp((*existing)->parent, item_handle->parent) == 0 &&
            strcmp((*existing)->internal_name, item_handle->internal_name) == 0) {
            return false;  // 重复注册
            }
    }

    // 加入 registry
    return tefstd_vector_push_back(&g_terraria_item_registry, &item_handle);
}

bool terraria_item_manager_unregister_item_by_id(const int runtime_id) {
    if (runtime_id == 0) return false;

    const size_t count = tefstd_vector_size(&g_terraria_item_registry);
    for (size_t i = 0; i < count; i++) {
        return tefstd_vector_erase(&g_terraria_item_registry, runtime_id, NULL);
    }
    return false;
}

bool terraria_item_manager_assign_ids(const int base_id) {
    const size_t count = tefstd_vector_size(&g_terraria_item_registry);
    if (count == 0) return true;

    // 获取底层数组指针进行排序
    terraria_item_handle_t** items = g_terraria_item_registry.data;

    // 排序
    qsort(items, count, sizeof(terraria_item_handle_t*), compare_item);

    // 分配 ID（从 base_id 开始紧凑分配）
    for (size_t i = 0; i < count; i++)
        items[i]->runtime_id = base_id + (int)i;

    g_ids_assigned = true;
    return true;
}

int terraria_item_id_get_count() {
    short count = 0;
    patchlib_field_get_value(item_id_count, NULL, &count);
    return count;
}

void terraria_item_manager_init_texture2d() {
    TEKLOG_DEBUG("terraria_item_manager_init_texture2d: starting");

    // 检查字段是否有效
    if (!patchlib_is_valid(f_texture_item) || !patchlib_is_valid(f_texture_item_flame)) {
        TEKLOG_ERROR("Texture fields not initialized");
        return;
    }

    patch_handle_t texture_item_array = PATCH_NULL;
    patch_handle_t texture_item_flame_array = PATCH_NULL;
    patchlib_field_get_value(f_texture_item, NULL, &texture_item_array);
    patchlib_field_get_value(f_texture_item_flame, NULL, &texture_item_flame_array);

    if (!patchlib_is_valid(texture_item_array) || !patchlib_is_valid(texture_item_flame_array)) {
        TEKLOG_ERROR("Failed to get texture arrays");
        return;
    }

    int base_count = terraria_item_id_get_count();
    size_t custom_count = tefstd_vector_size(&g_terraria_item_registry);
    size_t array_len = patchlib_array_length(texture_item_array);

    TEKLOG_INFO("init_texture2d: array_len=%zu, base_count=%d, custom_count=%zu, start_index=%d",
                array_len, base_count, custom_count, base_count);

    // 检查数组是否足够大
    if (base_count + (int)custom_count > array_len) {
        // PC端 LoadTextures postfix 可能在 Initialize_AlmostEverything（数组扩容）之前触发，
        // 此时数组还未扩容，属于正常情况，等待后续扩容后再写入贴图即可
        TEKLOG_DEBUG("Array too small: need %d, have %zu (arrays not resized yet)", base_count + (int)custom_count, array_len);
        return;
    }

    // 创建默认纹理
    patch_handle_t unknow_texture = unknown_item_get_texture(NULL);
    if (!patchlib_is_valid(unknow_texture)) {
        TEKLOG_ERROR("Failed to create default texture");
        return;
    }
    TEKLOG_DEBUG("Created default texture: %p", unknow_texture);

    int success_count = 0;
    int fail_count = 0;

    for (size_t i = 0; i < custom_count; ++i) {
        const size_t index = (size_t)base_count + i;

        terraria_item_handle_t** item_handle_ptr = tefstd_vector_at(&g_terraria_item_registry, i);
        if (!item_handle_ptr || !*item_handle_ptr) {
            TEKLOG_WARN("init_texture2d: invalid item_handle at index %zu", i);
            fail_count++;
            continue;
        }

        terraria_item_handle_t* item_handle = *item_handle_ptr;
        TEKLOG_DEBUG("init_texture2d: processing item %zu: %s.%s (index=%zu)",
                     i, item_handle->parent, item_handle->internal_name, index);

        patch_handle_t itx = PATCH_NULL;
        if (item_handle->item_ops.get_texture) {
            itx = item_handle->item_ops.get_texture(item_handle);
            TEKLOG_DEBUG("init_texture2d: get_texture returned %p for %s.%s",
                         itx, item_handle->parent, item_handle->internal_name);
        }

        if (patchlib_is_valid(itx)) {
            if (patchlib_array_set(texture_item_array, index, &itx)) {
                patchlib_array_set(texture_item_flame_array, index, &itx);
                success_count++;
                TEKLOG_DEBUG("init_texture2d: set custom texture for %s.%s at index %zu",
                             item_handle->parent, item_handle->internal_name, index);
            } else {
                TEKLOG_ERROR("init_texture2d: failed to set texture at index %zu", index);
                fail_count++;
            }
            patchlib_free(itx);
        } else {
            // 使用默认纹理
            if (patchlib_array_set(texture_item_array, index, &unknow_texture)) {
                patchlib_array_set(texture_item_flame_array, index, &unknow_texture);
                TEKLOG_DEBUG("init_texture2d: set default texture for %s.%s at index %zu",
                             item_handle->parent, item_handle->internal_name, index);
            } else {
                TEKLOG_ERROR("init_texture2d: failed to set default texture at index %zu", index);
                fail_count++;
            }
        }
    }

    TEKLOG_INFO("init_texture2d: completed: %d success, %d failed, %zu total custom items",
                success_count, fail_count, custom_count);

    patchlib_free(unknow_texture);
    patchlib_free(texture_item_array);
    patchlib_free(texture_item_flame_array);
}

static void resize_array(const char* ns, const char* cls, const char* fid,
                         patch_handle_t type, const int nsize) {
    // 获取类型和字段
    patch_handle_t class_handle = patchlib_type_get_type(ns, cls);
    if (!patchlib_is_valid(class_handle)) {
        TEKLOG_ERROR("resize_array: Failed to get class: %s.%s", ns, cls);
        return;
    }

    patch_handle_t field = patchlib_type_get_field(class_handle, fid);
    if (!patchlib_is_valid(field)) {
        TEKLOG_ERROR("resize_array: Failed to get field: %s.%s.%s", ns, cls, fid);
        patchlib_free(class_handle);
        return;
    }

    // 获取旧数组
    patch_handle_t array = PATCH_NULL;
    patchlib_field_get_value(field, NULL, &array);

    if (!patchlib_is_valid(array)) {
        TEKLOG_WARN("resize_array: Array %s.%s.%s is NULL, creating new array of size %d", ns, cls, fid, nsize);
        patch_handle_t narray = patchlib_array_create(nsize, type);
        if (patchlib_is_valid(narray)) {
            patchlib_field_set_value(field, NULL, &narray);
            TEKLOG_INFO("resize_array: Created new array %s.%s.%s of size %d", ns, cls, fid, nsize);
            patchlib_free(narray);
        }
        patchlib_free(field);
        patchlib_free(class_handle);
        return;
    }

    uint32_t old_len = patchlib_array_length(array);

    if (old_len >= (uint32_t)nsize) {
        TEKLOG_DEBUG("resize_array: %s.%s.%s already large enough: %u >= %d", ns, cls, fid, old_len, nsize);
        patchlib_free(array);
        patchlib_free(field);
        patchlib_free(class_handle);
        return;
    }

    TEKLOG_DEBUG("resize_array: Resizing %s.%s.%s from %u to %d", ns, cls, fid, old_len, nsize);

    // 创建新数组
    patch_handle_t narray = patchlib_array_create(nsize, type);
    if (!patchlib_is_valid(narray)) {
        TEKLOG_ERROR("resize_array: Failed to create new array of size %d for %s.%s.%s", nsize, ns, cls, fid);
        patchlib_free(array);
        patchlib_free(field);
        patchlib_free(class_handle);
        return;
    }

    // 复制所有旧数据到新数组
    if (old_len > 0) {
        if (!patchlib_array_copy(narray, array, old_len)) {
            TEKLOG_ERROR("resize_array: Failed to copy data for %s.%s.%s", ns, cls, fid);
        }
    }

    // 更新字段
    patchlib_field_set_value(field, NULL, &narray);
    TEKLOG_INFO("resize_array: ✅ %s.%s.%s resized from %u to %d", ns, cls, fid, old_len, nsize);

    // 清理资源
    patchlib_free(narray);
    patchlib_free(array);
    patchlib_free(field);
    patchlib_free(class_handle);
}

void terraria_item_manager_resize() {
    const int new_size = terraria_item_manager_get_count();

    patch_handle_t bool_type = patchlib_get_basic_type(PATCH_BOOL);
    patch_handle_t texture_type = terraria_texture2d_get_class();
    patch_handle_t asset_type = terraria_asset_get_generic_class(texture_type);

    // 注意：这些数组在 Terraria 中用于存储物品相关的数据
    resize_array("Terraria", "Item", "claw", bool_type, new_size);
    resize_array("Terraria", "Item", "staff", bool_type, new_size);


#if __ANDROID__
    resize_array("Terraria", "Player", "ItemUsesRightFire", bool_type, new_size);
    resize_array("", "VirtualControllerInputState", "ItemCategories", patchlib_get_basic_type(PATCH_INT32), new_size);
#endif

    // 引用类型数组 - 使用 PATCH_OBJECT（但实际类型需要匹配）
    resize_array("Terraria.GameContent", "TextureAssets", "Item", asset_type, new_size);
    resize_array("Terraria.GameContent", "TextureAssets", "ItemFlame", asset_type, new_size);

#if !__ANDROID__
    // PC端：一次性扩容 ItemID.Sets 所有按 ItemID.Count 定长的数组
    resize_all_itemid_sets(terraria_item_id_get_count(), new_size);
    // TextureCopyLoad 新增区域需填 -1（通用扩容后此函数只负责填充）
    resize_texture_copy_load(new_size);
    // PC端 绘制物品图标时按 type 索引 Main.itemAnimations，必须同步扩充
    resize_item_animations(new_size);
#endif

    patchlib_free(bool_type);
    patchlib_free(texture_type);
    patchlib_free(asset_type);
}

int terraria_item_manager_get_count() {
    return (int)tefstd_vector_size(&g_terraria_item_registry) + terraria_item_id_get_count();
}