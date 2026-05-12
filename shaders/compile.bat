C:/VulkanSDK/1.4.341.1/Bin/slangc.exe shader.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o slang.spv

:: this does nothing because apparently our cmake runs this exact same command -- i suppose we're including it just to... manually run it?
:: nevermind, w/ the compute shaders, i suppose this isnt working properly, whatever, idc, just manually run the thing, all we had to change is specify -entry compMain.

C:/VulkanSDK/1.4.341.1/Bin/slangc.exe compute.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry compMain -o compute.spv