mkdir external/
cd external/

# GLM
git clone https://github.com/g-truc/glm.git
cd glm/
git checkout -b target 0.9.8.4
cd ..

# GL3W
git clone https://github.com/skaslev/gl3w.git

# GLFW
git clone https://github.com/glfw/glfw.git
cd glfw/
git checkout -b target 3.2.1
cd ..

# LUT
git clone $(pwd)/../../lut