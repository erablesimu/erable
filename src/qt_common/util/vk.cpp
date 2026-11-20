// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <thread>
#include <utility>
#include <vector>

#include "qt_common/qt_common.h"

#include "common/dynamic_library.h"
#include "common/logging.h"
#include "common/memory_detect.h"
#include "qt_common/util/vk.h"
#include "video_core/fsr4_support.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_instance.h"
#include "video_core/vulkan_common/vulkan_library.h"
#include "video_core/vulkan_common/vulkan_surface.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"
#include "vulkan/vulkan_core.h"

class QWindow;

namespace VkDeviceInfo {
Record::Record(std::string_view name_, const std::vector<VkPresentModeKHR>& vsync_modes_,
               bool has_broken_compute_, bool fsr4_capable_)
    : name{name_}, vsync_support{vsync_modes_}, has_broken_compute{has_broken_compute_},
      fsr4_capable{fsr4_capable_} {}

Record::~Record() = default;

void PopulateRecords(std::vector<Record>& records, QWindow* window) try {
    using namespace Vulkan;

    // Create a test window with a Vulkan surface type for checking present modes.
    QWindow test_window(window);
    test_window.setSurfaceType(QWindow::VulkanSurface);
    test_window.create();
    auto wsi = QtCommon::GetWindowSystemInfo(&test_window);

    vk::InstanceDispatch dld;
    const auto library = OpenLibrary();
    const vk::Instance instance = CreateInstance(*library, dld, VK_API_VERSION_1_1, wsi.type);
    const std::vector<VkPhysicalDevice> physical_devices = instance.EnumeratePhysicalDevices();
    vk::SurfaceKHR surface = CreateSurface(instance, wsi);

    records.clear();
    records.reserve(physical_devices.size());
    for (const VkPhysicalDevice device : physical_devices) {
        const auto physical_device = vk::PhysicalDevice(device, dld);
        std::string name = physical_device.GetProperties().deviceName;

        const std::vector<VkPresentModeKHR> present_modes =
            physical_device.GetSurfacePresentModesKHR(*surface);

        VkPhysicalDeviceDriverProperties driver_properties{};
        driver_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        driver_properties.pNext = nullptr;
        VkPhysicalDeviceProperties2 properties{};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
        properties.pNext = &driver_properties;
        dld.vkGetPhysicalDeviceProperties2(physical_device, &properties);

        const auto driverID = driver_properties.driverID;

        bool has_broken_compute{
            Vulkan::Device::CheckBrokenCompute(driverID, properties.properties.driverVersion)};

#if defined(__linux__) && !defined(__ANDROID__)
        VkPhysicalDeviceShaderIntegerDotProductProperties dot_properties{};
        dot_properties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_PROPERTIES;
        if (properties.properties.apiVersion >= VK_API_VERSION_1_3) {
            VkPhysicalDeviceProperties2 dot_query{};
            dot_query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            dot_query.pNext = &dot_properties;
            dld.vkGetPhysicalDeviceProperties2(physical_device, &dot_query);
        }
        const bool fsr4_capable = VideoCommon::IsFsr4PortGpu(
            properties.properties.vendorID, properties.properties.deviceID,
            driverID == VK_DRIVER_ID_MESA_RADV,
            dot_properties.integerDotProduct4x8BitPackedMixedSignednessAccelerated != VK_FALSE);
#else
        const bool fsr4_capable = VideoCommon::IsFsr4CapableGpu(
            properties.properties.vendorID, driverID == VK_DRIVER_ID_AMD_PROPRIETARY, name);
#endif

        std::string driver_string = Vulkan::vk::GetDriverName(driver_properties);

        if (driver_string.empty())
            driver_string = "Unknown";

        name = fmt::format("{} ({})", name, driver_string);

        records.push_back(
            VkDeviceInfo::Record(name, present_modes, has_broken_compute, fsr4_capable));
    }
} catch (const Vulkan::vk::Exception& exception) {
    LOG_ERROR(Frontend, "Failed to enumerate devices with error: {}", exception.what());
}

std::optional<Settings::HostHardware> ProbeHostHardware(u32 device_index) try {
    using namespace Vulkan;

    vk::InstanceDispatch dld;
    const auto library = OpenLibrary();
    const vk::Instance instance = CreateInstance(*library, dld, VK_API_VERSION_1_1);
    const std::vector<VkPhysicalDevice> physical_devices = instance.EnumeratePhysicalDevices();
    if (device_index >= physical_devices.size()) {
        return std::nullopt;
    }
    const VkPhysicalDevice device = physical_devices[device_index];
    const auto physical_device = vk::PhysicalDevice(device, dld);
    const VkPhysicalDeviceProperties properties = physical_device.GetProperties();
    const std::string name = properties.deviceName;

    VkPhysicalDeviceDriverProperties driver_properties{};
    driver_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
    VkPhysicalDeviceProperties2 properties2{};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &driver_properties;
    dld.vkGetPhysicalDeviceProperties2(device, &properties2);

    VkPhysicalDeviceShaderFloat16Int8Features float16{};
    float16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &float16;
    dld.vkGetPhysicalDeviceFeatures2(device, &features);

    VkPhysicalDeviceMemoryProperties memory{};
    dld.vkGetPhysicalDeviceMemoryProperties(device, &memory);
    u64 vram = 0;
    for (u32 i = 0; i < memory.memoryHeapCount; ++i) {
        if ((memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
            vram = (std::max)(vram, u64{memory.memoryHeaps[i].size});
        }
    }

    constexpr u32 VendorNvidia = 0x10DE;
    constexpr u32 VendorAmd = 0x1002;
    Settings::HostHardware hw{
        .gpu_vendor_id = properties.vendorID,
        .vram_bytes = vram,
        .integrated = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
        // As vulkan_device.cpp tells them apart: no float16
        .old_amd = properties.vendorID == VendorAmd && float16.shaderFloat16 == VK_FALSE,
        .dlss = properties.vendorID == VendorNvidia && name.find("RTX") != std::string::npos,
        .fsr4 = VideoCommon::IsFsr4CapableGpu(
            properties.vendorID,
            driver_properties.driverID == VK_DRIVER_ID_AMD_PROPRIETARY, name),
        .cpu_threads = std::thread::hardware_concurrency(),
        .ram_bytes = Common::GetMemInfo().TotalPhysicalMemory,
    };
    LOG_INFO(Frontend,
             "Recommended settings for {}: {} MiB of VRAM{}{}, {} CPU threads, {} MiB of RAM",
             name, hw.vram_bytes >> 20, hw.integrated ? ", integrated" : "",
             hw.old_amd ? ", GCN4 or older" : "", hw.cpu_threads, hw.ram_bytes >> 20);
    return hw;
} catch (const Vulkan::vk::Exception& exception) {
    LOG_ERROR(Frontend, "Could not look at the GPU for the recommended settings: {}",
              exception.what());
    return std::nullopt;
}

} // namespace VkDeviceInfo
