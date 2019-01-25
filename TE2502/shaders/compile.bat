del "compiled\" /q

for %%f in (*.glsl) do (
@C:/VulkanSDK/1.1.82.1/Bin/glslangValidator.exe -V %%f -o compiled/%%f.spv
)
pause