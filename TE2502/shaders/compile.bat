if exist "compiled\" del "compiled\" /q
if not exist "compiled\" mkdir "compiled"

for %%f in (*.glsl) do (
@./glslangValidator.exe -V %%f -o compiled/%%f.spv
)
pause