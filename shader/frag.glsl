 #version 330
uniform int type;
uniform vec4 color;
in vec2 uv;
out vec4 out_color;
void main() {
    out_color = color;
    switch (type) {
        case 1:
            if (length(uv) > 1) out_color.a = 0;
            break;
        case 0:
            break;
    }
}
