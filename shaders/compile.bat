C:/VulkanSDK/1.4.341.1/Bin/slangc.exe shader.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o slang.spv
C:/VulkanSDK/1.4.341.1/Bin/slangc.exe particle_compute.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry compMain -o particle_compute.spv
C:/VulkanSDK/1.4.341.1/Bin/slangc.exe particle_graphics.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o particle_graphics.spv

C:/VulkanSDK/1.4.341.1/Bin/slangc.exe test.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o test.spv

C:/VulkanSDK/1.4.341.1/Bin/slangc.exe wireframe.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o wireframe.spv

C:/VulkanSDK/1.4.341.1/Bin/slangc.exe shadow_screen_vertex.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -o shadow_screen_vertex.spv
C:/VulkanSDK/1.4.341.1/Bin/slangc.exe shadow_screen_fragment.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry fragMain -o shadow_screen_fragment.spv

C:/VulkanSDK/1.4.341.1/Bin/slangc.exe shadow_offscreen_vertex.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -o shadow_offscreen_vertex.spv