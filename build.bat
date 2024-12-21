cpp -E -P shader\frag.glsl.src > shader\frag.glsl
gcc -Wall -Wextra -o main main.c -lgdi32 -lopengl32 -lwinmm