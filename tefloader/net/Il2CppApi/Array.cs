// /*******************************************************************************
//  * tefloader - Array.cs
//  * Copyright (C) 2026 eternalfuture-e38299
//  *
//  * This program is free software: you can redistribute it and/or modify
//  * it under the terms of the GNU Affero General Public License as published by
//  * the Free Software Foundation, either version 3 of the License, or
//  * (at your option) any later version.
//  *
//  * This program is distributed in the hope that it will be useful,
//  * but WITHOUT ANY WARRANTY; without even the implied warranty of
//  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  * GNU Affero General Public License for more details.
//  *
//  * You should have received a copy of the GNU Affero General Public License
//  * along with this program. If not, see <https://www.gnu.org/licenses/>.
//  *
//  * Author: eternalfuture-e38299
//  * GitHub: https://github.com/eternalfuture-e38299
//  * Created: $[InvalidReference]
//  *******************************************************************************/

using System.Runtime.InteropServices;

namespace tefloader.Il2CppApi;

public static class Array
{
    // ==================== 基础 API ====================

    // 模拟 il2cpp_array_new - 创建新数组（保持不变）
    public static IntPtr il2cpp_array_new(IntPtr elementTypeInfo, int length)
    {
        if (elementTypeInfo == IntPtr.Zero || length <= 0)
            return IntPtr.Zero;

        try
        {
            var typeHandle = GCHandle.FromIntPtr(elementTypeInfo);
            var elementType = (Type)typeHandle.Target;

            // 创建托管数组
            var managedArray = System.Array.CreateInstance(elementType, length);

            var arrayHandle = GCHandle.Alloc(managedArray);
            return GCHandle.ToIntPtr(arrayHandle);
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to create array: {ex.Message}");
            return IntPtr.Zero;
        }
    }

    // 模拟 il2cpp_array_element_size - 获取元素大小（保持不变）
    public static int il2cpp_array_element_size(IntPtr arrayPtr)
    {
        if (arrayPtr == IntPtr.Zero)
            return 0;

        try
        {
            var arrayHandle = GCHandle.FromIntPtr(arrayPtr);
            var array = (System.Array)arrayHandle.Target;

            var elementType = array.GetType().GetElementType();
            return elementType == null ? 0 : Marshal.SizeOf(elementType);
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to get element size: {ex.Message}");
            return 0;
        }
    }

    // 模拟 il2cpp_array_length - 获取数组总长度（保持不变）
    public static int il2cpp_array_length(IntPtr arrayPtr)
    {
        if (arrayPtr == IntPtr.Zero)
            return 0;

        try
        {
            var arrayHandle = GCHandle.FromIntPtr(arrayPtr);
            var array = (System.Array)arrayHandle.Target;

            return array.Length;
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to get array length: {ex.Message}");
            return 0;
        }
    }

    // il2cpp_array_resize - 扩容数组：创建新数组并复制旧数据，返回新数组句柄（失败返回 IntPtr.Zero）
    public static IntPtr il2cpp_array_resize(IntPtr arrayPtr, int newSize)
    {
        if (arrayPtr == IntPtr.Zero || newSize <= 0)
            return IntPtr.Zero;

        try
        {
            var arrayHandle = GCHandle.FromIntPtr(arrayPtr);
            var array = (System.Array)arrayHandle.Target;

            if (array == null || newSize <= array.Length)
                return IntPtr.Zero;

            var elementType = array.GetType().GetElementType();
            if (elementType == null)
                return IntPtr.Zero;

            var newArray = System.Array.CreateInstance(elementType, newSize);
            System.Array.Copy(array, newArray, array.Length);

            var newHandle = GCHandle.Alloc(newArray);
            return GCHandle.ToIntPtr(newHandle);
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to resize array: {ex.Message}");
            return IntPtr.Zero;
        }
    }

    // ==================== 辅助方法：多维索引转换 ====================

    // 将一维索引转换为多维索引（行主序，C语言风格）
    private static int[] GetMultiDimensionalIndices(System.Array array, int flatIndex)
    {
        if (array.Rank == 1)
            return [flatIndex];

        var indices = new int[array.Rank];
        var remaining = flatIndex;

        // 从最后一维开始计算（行主序）
        for (var i = array.Rank - 1; i >= 0; i--)
        {
            var dimSize = array.GetLength(i);
            indices[i] = remaining % dimSize;
            remaining /= dimSize;
        }

        return indices;
    }

    // ==================== 元素操作 ====================

    // 额外功能：il2cpp_array_at - 获取数组元素（支持多维数组的一维索引访问）
    public static unsafe bool il2cpp_array_at(IntPtr arrayPtr, int index, void* outValue)
    {
        if (arrayPtr == IntPtr.Zero || outValue == null || index < 0)
            return false;

        try
        {
            var arrayHandle = GCHandle.FromIntPtr(arrayPtr);
            var array = (System.Array)arrayHandle.Target;

            if (index >= array.Length)
                return false;

            // 如果是多维数组，将一维索引转换为多维索引
            object value;
            if (array.Rank > 1)
            {
                var indices = GetMultiDimensionalIndices(array, index);
                value = array.GetValue(indices);
            }
            else
            {
                value = array.GetValue(index);
            }

            if (value == null)
                return false;

            var elementType = array.GetType().GetElementType();

            if (elementType!.IsValueType)
                // 值类型直接复制
            {
                Marshal.StructureToPtr(value, (IntPtr)outValue, false);
            }
            else
            {
                // 引用类型，分配 GCHandle
                var handle = GCHandle.Alloc(value);
                *(IntPtr*)outValue = GCHandle.ToIntPtr(handle);
            }

            return true;
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to get array element: {ex.Message}");
            return false;
        }
    }

    // 额外功能：il2cpp_array_set - 设置数组元素（支持多维数组的一维索引访问）
    public static unsafe bool il2cpp_array_set(IntPtr arrayPtr, int index, void* value)
    {
        if (arrayPtr == IntPtr.Zero || value == null || index < 0)
            return false;

        try
        {
            var arrayHandle = GCHandle.FromIntPtr(arrayPtr);
            var array = (System.Array)arrayHandle.Target;

            if (index >= array.Length)
                return false;

            var elementType = array.GetType().GetElementType();
            object? newValue;

            if (elementType!.IsValueType)
            {
                // 值类型从指针读取
                newValue = Marshal.PtrToStructure((IntPtr)value, elementType);
            }
            else
            {
                // 引用类型从 GCHandle 获取
                var objPtr = *(IntPtr*)value;
                if (objPtr == IntPtr.Zero)
                    newValue = null;
                else
                    newValue = Utils.PtrToObject(objPtr);
            }

            // 如果是多维数组，将一维索引转换为多维索引
            if (array.Rank > 1)
            {
                var indices = GetMultiDimensionalIndices(array, index);
                array.SetValue(newValue, indices);
            }
            else
            {
                array.SetValue(newValue, index);
            }

            return true;
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to set array element: {ex.Message}");
            return false;
        }
    }

    // 额外功能：il2cpp_array_fill - 填充数组（支持多维数组）
    public static unsafe bool il2cpp_array_fill(IntPtr arrayPtr, void* value)
    {
        if (arrayPtr == IntPtr.Zero || value == null)
            return false;

        try
        {
            var arrayHandle = GCHandle.FromIntPtr(arrayPtr);
            var array = (System.Array)arrayHandle.Target;

            var elementType = array.GetType().GetElementType();

            if (elementType!.IsValueType)
            {
                // 值类型：读取一次值，然后填充所有元素
                var fillValue = Marshal.PtrToStructure((IntPtr)value, elementType);
                for (var i = 0; i < array.Length; i++)
                    if (array.Rank > 1)
                    {
                        var indices = GetMultiDimensionalIndices(array, i);
                        array.SetValue(fillValue, indices);
                    }
                    else
                    {
                        array.SetValue(fillValue, i);
                    }
            }
            else
            {
                // 引用类型：从 GCHandle 获取对象
                var objPtr = *(IntPtr*)value;
                var fillValue = objPtr == IntPtr.Zero ? null : Utils.PtrToObject(objPtr);
                for (var i = 0; i < array.Length; i++)
                    if (array.Rank > 1)
                    {
                        var indices = GetMultiDimensionalIndices(array, i);
                        array.SetValue(fillValue, indices);
                    }
                    else
                    {
                        array.SetValue(fillValue, i);
                    }
            }

            return true;
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to fill array: {ex.Message}");
            return false;
        }
    }

    // ==================== C 数组与托管数组之间的复制 ====================

    // 额外功能：il2cpp_array_copy_from_c - 从 C 数组复制到托管数组（支持引用类型）
    public static unsafe bool il2cpp_array_copy_from_c(IntPtr destArrayPtr, void* src, int count)
    {
        if (destArrayPtr == IntPtr.Zero || src == null || count <= 0)
            return false;

        try
        {
            var arrayHandle = GCHandle.FromIntPtr(destArrayPtr);
            var destArray = (System.Array)arrayHandle.Target;

            if (destArray == null)
                return false;

            if (count > destArray.Length)
                count = destArray.Length;

            var elementType = destArray.GetType().GetElementType();

            // 检查元素类型
            if (elementType == null)
                return false;

            // 获取元素大小（用于值类型）
            int elementSize;
            var isValueType = elementType.IsValueType;

            if (isValueType)
            {
                elementSize = Marshal.SizeOf(elementType);

                // 值类型：直接从内存复制
                for (var i = 0; i < count; i++)
                {
                    var elementPtr = (IntPtr)src + i * elementSize;
                    var value = Marshal.PtrToStructure(elementPtr, elementType);

                    if (destArray.Rank > 1)
                    {
                        var indices = GetMultiDimensionalIndices(destArray, i);
                        destArray.SetValue(value, indices);
                    }
                    else
                    {
                        destArray.SetValue(value, i);
                    }
                }
            }
            else
            {
                // 引用类型：从 GCHandle 指针数组复制
                // 注意：src 指向的是 GCHandle 指针数组，每个元素是 IntPtr
                for (var i = 0; i < count; i++)
                {
                    // 读取 GCHandle 指针
                    var handlePtr = *(IntPtr*)((byte*)src + i * IntPtr.Size);
                    object? value;

                    if (handlePtr == IntPtr.Zero)
                        value = null;
                    else
                        try
                        {
                            // 从 GCHandle 获取对象
                            var handle = GCHandle.FromIntPtr(handlePtr);
                            value = handle.Target;
                        }
                        catch
                        {
                            // 如果无法从 GCHandle 获取，尝试直接转换指针为对象
                            value = Utils.PtrToObject(handlePtr);
                        }

                    if (destArray.Rank > 1)
                    {
                        var indices = GetMultiDimensionalIndices(destArray, i);
                        destArray.SetValue(value, indices);
                    }
                    else
                    {
                        destArray.SetValue(value, i);
                    }
                }
            }

            return true;
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to copy from C array: {ex.Message}");
            return false;
        }
    }

// 额外功能：il2cpp_array_copy_to_c - 从托管数组复制到 C 数组（支持引用类型）
    public static unsafe bool il2cpp_array_copy_to_c(void* dest, IntPtr srcArrayPtr, int count)
    {
        if (dest == null || srcArrayPtr == IntPtr.Zero || count <= 0)
            return false;

        try
        {
            var arrayHandle = GCHandle.FromIntPtr(srcArrayPtr);
            var srcArray = (System.Array)arrayHandle.Target;

            if (srcArray == null)
                return false;

            if (count > srcArray.Length)
                count = srcArray.Length;

            var elementType = srcArray.GetType().GetElementType();

            if (elementType == null)
                return false;

            var isValueType = elementType.IsValueType;

            if (isValueType)
            {
                // 值类型：直接复制到内存
                var elementSize = Marshal.SizeOf(elementType);

                for (var i = 0; i < count; i++)
                {
                    object? value;
                    if (srcArray.Rank > 1)
                    {
                        var indices = GetMultiDimensionalIndices(srcArray, i);
                        value = srcArray.GetValue(indices);
                    }
                    else
                    {
                        value = srcArray.GetValue(i);
                    }

                    var elementPtr = (IntPtr)dest + i * elementSize;

                    if (value != null)
                        Marshal.StructureToPtr(value, elementPtr, false);
                    else
                        // 如果是 null，清零
                        *(long*)elementPtr = 0;
                }
            }
            else
            {
                // 引用类型：存储 GCHandle 指针到 C 数组
                for (var i = 0; i < count; i++)
                {
                    object? value;
                    if (srcArray.Rank > 1)
                    {
                        var indices = GetMultiDimensionalIndices(srcArray, i);
                        value = srcArray.GetValue(indices);
                    }
                    else
                    {
                        value = srcArray.GetValue(i);
                    }

                    var destPtr = (IntPtr*)dest + i;

                    if (value == null)
                    {
                        *destPtr = IntPtr.Zero;
                    }
                    else
                    {
                        // 分配 GCHandle 并返回指针
                        var handle = GCHandle.Alloc(value);
                        *destPtr = GCHandle.ToIntPtr(handle);
                    }
                }
            }

            return true;
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to copy to C array: {ex.Message}");
            return false;
        }
    }

    // il2cpp_array_copy - 直接从源数组复制到目标数组（高性能版本）
    public static bool il2cpp_array_copy(IntPtr destArrayPtr, IntPtr srcArrayPtr, int count)
    {
        if (destArrayPtr == IntPtr.Zero || srcArrayPtr == IntPtr.Zero)
            return false;

        try
        {
            var destHandle = GCHandle.FromIntPtr(destArrayPtr);
            var srcHandle = GCHandle.FromIntPtr(srcArrayPtr);

            var destArray = (System.Array)destHandle.Target;
            var srcArray = (System.Array)srcHandle.Target;

            if (destArray == null || srcArray == null)
                return false;

            // 自动适应
            count = count <= 0
                ? Math.Min(destArray.Length, srcArray.Length)
                : Math.Min(count, Math.Min(destArray.Length, srcArray.Length));

            if (count <= 0)
                return true;

            var destElementType = destArray.GetType().GetElementType();
            var srcElementType = srcArray.GetType().GetElementType();

            // 如果类型相同且是一维数组，使用 Array.Copy（最快）
            if (destElementType == srcElementType && destArray.Rank == 1 && srcArray.Rank == 1)
            {
                System.Array.Copy(srcArray, 0, destArray, 0, count);
                return true;
            }

            // 如果类型相同但可能是多维数组，使用 Array.Copy 也能处理
            if (destElementType == srcElementType)
            {
                // Array.Copy 支持多维数组
                System.Array.Copy(srcArray, 0, destArray, 0, count);
                return true;
            }

            // 类型不同，需要逐元素转换（保持原有逻辑）
            Logger.Warning($"Copying between different element types: {srcElementType} -> {destElementType}");

            for (var i = 0; i < count; i++)
            {
                object? value;

                // 从源数组读取
                if (srcArray.Rank > 1)
                {
                    var indices = GetMultiDimensionalIndices(srcArray, i);
                    value = srcArray.GetValue(indices);
                }
                else
                {
                    value = srcArray.GetValue(i);
                }

                // 类型转换
                if (value != null && value.GetType() != destElementType)
                    try
                    {
                        value = Convert.ChangeType(value, destElementType!);
                    }
                    catch
                    {
                        Logger.Warning($"Cannot convert {value.GetType()} to {destElementType}");
                    }

                // 写入目标数组
                if (destArray.Rank > 1)
                {
                    var indices = GetMultiDimensionalIndices(destArray, i);
                    destArray.SetValue(value, indices);
                }
                else
                {
                    destArray.SetValue(value, i);
                }
            }

            return true;
        }
        catch (Exception ex)
        {
            Logger.Error($"Failed to copy array: {ex.Message}");
            return false;
        }
    }
}