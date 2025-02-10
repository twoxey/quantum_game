g++ -Wall -Wextra -shared -o dynamic.dll dynamic.cpp
g++ -Wall -Wextra -o main main.cpp -lgdi32 -lopengl32 -lwinmm
