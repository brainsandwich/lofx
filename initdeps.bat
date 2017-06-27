md external
cd external

rem GLM
git clone https://github.com/g-truc/glm.git
cd glm
git fetch
git checkout -b target 0.9.8.4
git pull
cd ..

rem GL3W
git clone https://github.com/skaslev/gl3w.git
cd gl3w
git fetch
git checkout -b target master
git pull
cd ..\..
external\gl3w\gl3w_gen.py
cd external

rem GLFW
git clone https://github.com/glfw/glfw.git
cd glfw
git fetch
git checkout -b target 3.2.1
git pull
cd ..

rem LUT
git clone %cd%\..\..\lut
cd lut
git fetch
git checkout -b target master
git pull
cd ..