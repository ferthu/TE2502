if exist "compiled\" del "compiled\" /q
if not exist "compiled\" mkdir "compiled"

for %%f in (*.glsl) do (
@C:/VulkanSDK/1.1.82.1/Bin/glslangValidator.exe -V %%f -o compiled/%%f.spv
)
pause