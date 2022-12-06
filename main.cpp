#include <iostream>
#include <SDL2/SDL.h>
#include <limits>
#include <vector>
#include <algorithm>

#define scp(pointer, message) {                                               \
    if (pointer == NULL) {                                                    \
        SDL_Log("Error: %s! SDL_Error: %s", message, SDL_GetError()); \
        exit(1);                                                              \
    }                                                                         \
}

#define scc(code, message) {                                                  \
    if (code < 0) {                                                           \
        SDL_Log("Error: %s! SDL_Error: %s", message, SDL_GetError()); \
        exit(1);                                                              \
    }                                                                         \
}

SDL_Window * g_window;
SDL_Renderer *g_renderer;

int screen_width;
int screen_height;

void init() {

	scc(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER), "Could not initialize SDL");
    if(!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
        SDL_Log("Warning: Linear texture filtering not enabled!");
    }

    scp((g_window = SDL_CreateWindow("Can'ts", 0, 0, screen_width, screen_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE)),
            "Could not create window");

    scp((g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)),
            "Could not create renderer");

    SDL_SetRelativeMouseMode(SDL_TRUE);
}


typedef SDL_FPoint Point2D;

struct Point3D {
    float x;
    float y;
    float z;
};

std::ostream & operator<<(std::ostream &stream, const Point3D &p) {
    stream << "(" << p.x << ", " << p.y << ", " << p.z << ")";
    return stream;
}
std::ostream & operator<<(std::ostream &stream, const Point2D &p) {
    stream << "(" << p.x << ", " << p.y << ")";
    return stream;
}

typedef Point3D Face[4];

const Face cube_faces[] = {
  // Bottom
    {
        { -0.5,  0.5,  -0.5 },
        {  0.5,  0.5,  -0.5 },
        {  0.5,  0.5,  0.5 },
        {  -0.5,  0.5,  0.5 },
    },
  // Top
    {
    {  -0.5,  -0.5,  -0.5 },
    {  0.5,  -0.5,  -0.5 },
    {  0.5,  -0.5,  0.5 },
    {  -0.5,  -0.5,  0.5 },
  },
  // Front
    {
    {  -0.5,  -0.5,  0.5 },
    {  0.5,  -0.5,  0.5 },
    {  0.5,  0.5,  0.5 },
    {  -0.5,  0.5,  0.5 },
  },
  // Back
  {
    {  -0.5,  -0.5,  -0.5 },
    {  0.5,  -0.5,  -0.5 },
    {  0.5,  0.5,  -0.5 },
    {  -0.5,  0.5,  -0.5 },
  },
};


#define FOV_IN_DEGREES 120.0
const double FOV = FOV_IN_DEGREES * M_PI / 180;
const float tana2 = tan(FOV / 2);

struct Player {

    Player(float x, float y, float z) 
    : m_pos({x, y, z}), m_horizontal_view_angle(0.0), m_vertical_view_angle(0.0) {}

    void move_forward(float d) {
        m_pos.z += std::cos(m_horizontal_view_angle) * d;
        m_pos.x += std::sin(m_horizontal_view_angle) * d;
    }

    void move_backward(float d) {
        move_forward(-d);
    }

    void move_left(float d) {
        m_pos.z += std::sin(m_horizontal_view_angle) * d;
        m_pos.x -= std::cos(m_horizontal_view_angle) * d;
    }
    void move_right(float d) {
        move_left(-d);
    }

    void move_x(float dx) {
        m_pos.x += dx;
    }

    void move_y(float dy) {
        m_pos.y += dy;
    }

    void move_z(float dz) {
        m_pos.z += dz;
    }

    Point3D m_pos;
    float m_vertical_view_angle;
    float m_horizontal_view_angle;
};

Point3D operator*(const Point3D &p, float a) {
    return {p.x * a, p.y * a, p.z * a};
}

Point3D operator+(const Point3D &p1, const Point3D &p2) {
    return {p1.x + p2.x, p1.y + p2.y, p1.z + p2.z};
}
Point3D operator-(const Point3D &p1, const Point3D &p2) {
    return {p1.x - p2.x, p1.y - p2.y, p1.z - p2.z};
}

inline Point2D project(Point3D p3) {
    if (p3.z < 1) p3.z = 1;
    return { p3.x / (p3.z * tana2), p3.y / (p3.z * tana2)};
}

inline Point3D rotate_y(float angle, Point3D p) {
    return {p.x * cos(angle) + p.z * sin(angle), p.y, -sin(angle) * p.x + cos(angle) * p.z};
}

inline Point3D rotate_y_around_point(float angle, Point3D p, Point3D origin) {
    Point3D p1 = p - origin;
    p1 = rotate_y(angle, p1);
    return p1 + origin;
}

inline Point2D project(Point3D p3, Player &player) {
    p3 = p3 - player.m_pos;
    if (p3.z < 1) p3.z = 1;
    return { p3.x / (p3.z * tana2), p3.y / (p3.z * tana2)};
}

inline Point2D project_with_camera(Point3D p, Player& player) {
    return project(rotate_y_around_point(-player.m_horizontal_view_angle, p, player.m_pos), player);
}

inline Point2D place_projected_point(Point2D point) {
    return { std::clamp((point.x * screen_width) + screen_width * 0.5f,  -500.0f, (float) screen_width + 500),
             std::clamp((point.y * screen_width) + screen_height * 0.5f, -500.0f, (float) screen_height + 500) };
}

inline Point2D get_onscreen_point(Point3D p, Player &player) {
    return place_projected_point(project_with_camera(p, player));
}

struct Cube {
    Cube(Point3D pos, float scale) : m_pos(pos), m_scale(scale) {
        /*
         * pos   - position of the center of the cube
         * scale - the size of the cube
         *
         */
    }

    void draw(SDL_Renderer *renderer, Player &player) {
        //TODO: optimize drawing the same side twice
        for (int i = 0; i < 4; ++i) {
            Point2D pts[5];
            for (int j = 0; j < 4; ++j) {
                pts[j] = get_onscreen_point(cube_faces[i][j] * m_scale + m_pos, player);
            }

            if (!check_point(pts[0])) {
                return;
            }
            pts[4] = pts[0];

            SDL_RenderDrawLinesF(renderer, pts, 5);
        }
    }

    void move(float x, float y = 0, float z = 0) {
        m_pos.x += x;
        m_pos.y += y;
        m_pos.z += z;
    }

    Point3D getpos() {
        return m_pos;
    }

private:
    float m_scale;
    Point3D m_pos;

    bool check_point(Point2D &p) {
        return (-500 <= p.x && p.x <= screen_width + 500) && (-500 <= p.y <= screen_height + 500);
    }
};


struct Axes {

    Axes(Point3D pos, float length) : m_pos(pos), m_length(length) {}

    void draw(SDL_Renderer *renderer, Player &player) {
        
        Point2D origin = get_onscreen_point(m_pos, player);

        Point2D x = get_onscreen_point(m_pos + (Point3D) {1 * m_length, 0, 0}, player);
        Point2D y = get_onscreen_point(m_pos + (Point3D) {0, 1 * m_length, 0}, player);
        Point2D z = get_onscreen_point(m_pos + (Point3D) {0, 0, 1 * m_length}, player);

        // draw x axis
        SDL_RenderDrawLineF(renderer, origin.x, origin.y, x.x, x.y);
        SDL_RenderDrawLineF(renderer, x.x - 10, x.y - 20, x.x + 10, x.y);
        SDL_RenderDrawLineF(renderer, x.x - 10, x.y, x.x + 10, x.y - 20);

        // draw y axis
        SDL_RenderDrawLineF(renderer, origin.x, origin.y, y.x, y.y);
        SDL_RenderDrawLineF(renderer, y.x + 15, y.y,      y.x + 15, y.y - 10);
        SDL_RenderDrawLineF(renderer, y.x + 15, y.y - 10, y.x + 5,  y.y - 20);
        SDL_RenderDrawLineF(renderer, y.x + 15, y.y - 10, y.x + 25, y.y - 20);

        SDL_RenderDrawLineF(renderer, origin.x, origin.y, z.x, z.y);
        SDL_RenderDrawLineF(renderer, z.x - 10, z.y - 5, z.x + 10, z.y - 5);
        SDL_RenderDrawLineF(renderer, z.x + 10, z.y - 5, z.x - 10, z.y + 20);
        SDL_RenderDrawLineF(renderer, z.x - 10, z.y + 20, z.x + 10, z.y + 20);


    }

private:
    Point3D m_pos;
    float m_length;
};


int main () {
    bool quit;
    init();
    SDL_Event event;

    Player player(0, 0, 0);

    int input_timer_start = SDL_GetTicks();


#if 0
    Cube *cubes[16];

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            cubes[i * 4 + j] = new Cube({(float) i - 2, 1.1, 3 + (float) j}, 1);
        }
    }
#else
    Cube cube({0, 1.1, 3}, 1);
#endif

    Axes axes({1, 0.5, 3}, 1);


    while (!quit) {
        while(SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                case SDL_QUIT:
                    quit = true;
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        screen_width = event.window.data1;
                        screen_height = event.window.data2;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    player.m_vertical_view_angle   = std::clamp(player.m_vertical_view_angle + (double) event.motion.yrel / 400, -M_PI / 2, M_PI / 2); // can only look 90 degrees up
                    player.m_horizontal_view_angle = player.m_horizontal_view_angle + (double) event.motion.xrel / 400;
                    break;
            }
        }

        if (SDL_GetTicks() - input_timer_start > 1000 / 30) {
            input_timer_start = SDL_GetTicks();
            const Uint8* state = SDL_GetKeyboardState(NULL);
            if (state[SDL_SCANCODE_D]) {
                player.move_right(0.1);
            }
            if (state[SDL_SCANCODE_A]) {
                player.move_left(0.1);
            }
            if (state[SDL_SCANCODE_W]) {
                player.move_forward(0.1);
            }
            if (state[SDL_SCANCODE_S]) {
                player.move_backward(0.1);
            }
            if (state[SDL_SCANCODE_UP]) {
                player.move_y(-0.1);
            }
            if (state[SDL_SCANCODE_DOWN]) {
                player.move_y(0.1);
            }
        }

        SDL_SetRenderDrawColor(g_renderer, 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderClear(g_renderer);

        /***************** Drawing *******************************/

        SDL_SetRenderDrawColor(g_renderer, 0xFF, 0x33, 0x33, 0xFF);
#if 0
        for (int i = 0; i < 16; ++i) {
            cubes[i]->draw(g_renderer, player);
        }
#else
        cube.draw(g_renderer, player);
#endif
        axes.draw(g_renderer, player);

        /*********************************************************/

        SDL_RenderPresent(g_renderer);
    }

    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}
