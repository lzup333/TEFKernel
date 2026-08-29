/*******************************************************************************
 * tefkernel - set_factory
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
 * Created: 2026/8/27
 *******************************************************************************/

#include "internal/terraria/set_factory.h"

#include "internal/log.h"
#include "internal/terraria/item_manager.h"
#include "patchlib/type.h"
#include "patchlib/method.h"

static bool set_factory_ctor_prefix(patch_handle_t instance, void **args,
                               const patch_method_signature_t *sig_info, void *result) {

    const int size = *(int*)args[0];

    int current_count = terraria_item_id_get_count();
    size_t custom_count = tefstd_vector_size(&g_terraria_item_registry);
    int new_size = current_count + (int)custom_count;

    TEKLOG_DEBUG("set_size_field: current size=%d, base_count=%d, custom_count=%zu, new_size=%d",
                 size, current_count, custom_count, new_size);

    if (size == current_count) {
        TEKLOG_INFO("SetFactory._size: %d -> %d (adding %zu custom items)",
                    size, new_size, custom_count);
        *(int*)args[0] = new_size;
    } else if (size == new_size) {
        TEKLOG_DEBUG("SetFactory._size already correct: %d", size);
    } else {
        TEKLOG_WARN("SetFactory._size mismatch: current=%d, expected=%d or %d",
                    size, current_count, new_size);
    }

    return false;
}

void terraria_set_factory_init() {
    TEKLOG_DEBUG("terraria_set_factory_init called");

    patch_handle_t set_factory_class = patchlib_type_get_type("Terraria.ID", "SetFactory");
    if (!patchlib_is_valid(set_factory_class)) {
        TEKLOG_ERROR("Failed to get SetFactory class");
        return;
    }
    TEKLOG_DEBUG("Got SetFactory class: %p", set_factory_class);

    patch_handle_t set_factory_ctor = patchlib_type_get_method_by_param_count(set_factory_class, ".ctor", 1);
    if (patchlib_is_valid(set_factory_ctor)) {
        patch_hook_id_t ctor_hook = patchlib_install_prepost_hook(
            set_factory_ctor, set_factory_ctor_prefix, NULL
        );
        TEKLOG_INFO("SetFactory constructor hook installed: %s (ID: %d)",
                    ctor_hook != PATCH_HOOK_INVALID_ID ? "success" : "failed", ctor_hook);
        patchlib_free(set_factory_ctor);
    } else {
        TEKLOG_WARN("Failed to get SetFactory constructor (may have parameters)");
    }

    patchlib_free(set_factory_class);

    TEKLOG_DEBUG("terraria_set_factory_init completed");
}

