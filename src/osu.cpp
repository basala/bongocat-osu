#include "header.hpp"

#if defined(__unix__) || defined(__unix)
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

extern "C" {
#include <xdo.h>
}
#else
#include <windows.h>
#endif

namespace osu {
Json::Value left_key_value, right_key_value, smoke_key_value, wave_key_value;
int osu_x, osu_y, osu_h, osu_v;
int offset_x, offset_y;
int horizontal, vertical;
int paw_r, paw_g, paw_b, paw_a;
int paw_edge_r, paw_edge_g, paw_edge_b, paw_edge_a;
double scale;
bool is_mouse, is_letterbox, is_left_handed, is_enable_toggle_smoke;
sf::Sprite bg, up, left, right, device, smoke, wave;

int key_state = 0;

bool left_key_state = false;
bool right_key_state = false;
bool wave_key_state = false;
bool previous_smoke_key_state = false;
bool current_smoke_key_state = false;
bool is_toggle_smoke = false;
double timer_left_key = -1;
double timer_right_key = -1;
double timer_wave_key = -1;

#if defined(__unix__) || defined(__unix)
xdo_t* xdo;
Window foreground_window;

static int _XlibErrorHandler(Display *display, XErrorEvent *event) {
    return true;
}
#endif

bool init() {
    // getting configs
    Json::Value osu = data::cfg["osu"];

    is_mouse = osu["mouse"].asBool();
    is_enable_toggle_smoke = osu["toggleSmoke"].asBool();

    paw_r = osu["paw"][0].asInt();
    paw_g = osu["paw"][1].asInt();
    paw_b = osu["paw"][2].asInt();
    paw_a = osu["paw"].size() == 3 ? 255 : osu["paw"][3].asInt();

    paw_edge_r = osu["pawEdge"][0].asInt();
    paw_edge_g = osu["pawEdge"][1].asInt();
    paw_edge_b = osu["pawEdge"][2].asInt();
    paw_edge_a = osu["pawEdge"].size() == 3 ? 255 : osu["pawEdge"][3].asInt();

    bool chk[256];
    std::fill(chk, chk + 256, false);
    left_key_value = osu["key1"];
    for (Json::Value &v : left_key_value) {
        chk[v.asInt()] = true;
    }
    right_key_value = osu["key2"];
    for (Json::Value &v : right_key_value) {
        if (chk[v.asInt()]) {
            data::error_msg("Overlapping osu! keybinds", "Error reading configs");
            return false;
        }
    }
    wave_key_value = osu["wave"];
    for (Json::Value &v : wave_key_value) {
        if (chk[v.asInt()]) {
            data::error_msg("Overlapping osu! keybinds", "Error reading configs");
            return false;
        }
    }
    smoke_key_value = osu["smoke"];

    is_letterbox = data::cfg["resolution"]["letterboxing"].asBool();
    osu_x = data::cfg["resolution"]["width"].asInt();
    osu_y = data::cfg["resolution"]["height"].asInt();
    osu_h = data::cfg["resolution"]["horizontalPosition"].asInt();
    osu_v = data::cfg["resolution"]["verticalPosition"].asInt();
    is_left_handed = data::cfg["decoration"]["leftHanded"].asBool();

    if (is_mouse) {
        offset_x = (data::cfg["decoration"]["offsetX"])[0].asInt();
        offset_y = (data::cfg["decoration"]["offsetY"])[0].asInt();
        scale = (data::cfg["decoration"]["scalar"])[0].asDouble();
    } else {
        offset_x = (data::cfg["decoration"]["offsetX"])[1].asInt();
        offset_y = (data::cfg["decoration"]["offsetY"])[1].asInt();
        scale = (data::cfg["decoration"]["scalar"])[1].asDouble();
    }

    // importing sprites
    up.setTexture(data::load_texture("img/osu/up.png"));
    left.setTexture(data::load_texture("img/osu/left.png"));
    right.setTexture(data::load_texture("img/osu/right.png"));
    wave.setTexture(data::load_texture("img/osu/wave.png"));
    if (is_mouse) {
        bg.setTexture(data::load_texture("img/osu/mousebg.png"));
        device.setTexture(data::load_texture("img/osu/mouse.png"), true);
    } else {
        bg.setTexture(data::load_texture("img/osu/tabletbg.png"));
        device.setTexture(data::load_texture("img/osu/tablet.png"), true);
    }
    smoke.setTexture(data::load_texture("img/osu/smoke.png"));
    device.setScale(scale, scale);

#if defined(__unix__) || defined(__unix)
    // Set x11 error handler
    XSetErrorHandler(_XlibErrorHandler);

    // Get desktop resolution
    int num_sizes;
    Rotation current_rotation;

    Display *dpy = XOpenDisplay(NULL);
    Window root = RootWindow(dpy, 0);
    XRRScreenSize *xrrs = XRRSizes(dpy, 0, &num_sizes);

    XRRScreenConfiguration *conf = XRRGetScreenInfo(dpy, root);
    SizeID current_size_id = XRRConfigCurrentConfiguration(conf, &current_rotation);

    int current_width = xrrs[current_size_id].width;
    int current_height = xrrs[current_size_id].height;

    XCloseDisplay(dpy);

    horizontal = current_width;
    vertical = current_height;

    xdo = xdo_new(NULL);
#else
    // getting resolution
    RECT desktop;
    const HWND h_desktop = GetDesktopWindow();
    GetWindowRect(h_desktop, &desktop);
    horizontal = desktop.right;
    vertical = desktop.bottom;
#endif

    return true;
}

std::pair<double, double> get_xy() {
#if defined(__unix__) || defined(__unix)
    double letter_x, letter_y, s_height, s_width;
    bool found_window = (xdo_get_focused_window_sane(xdo, &foreground_window) == 0);

    if (found_window) {
        unsigned char* name_ret;
        int name_len_ret;
        int name_type;

        xdo_get_window_name(xdo, foreground_window, &name_ret, &name_len_ret, &name_type);
        bool can_get_name = (name_len_ret > 0);

        if (can_get_name) {

            std::string title = "";

            if (name_ret != NULL)
            {
                std::string foreground_title(reinterpret_cast<char*>(name_ret));
                title = foreground_title;
            }

            if (title.find("osu!") == 0) {
                if (!is_letterbox) {

                    int x_ret;
                    int y_ret;
                    unsigned int width_ret;
                    unsigned int height_ret;

                    bool can_get_location = (xdo_get_window_location(xdo, foreground_window, &x_ret, &y_ret, NULL) == 0);
                    bool can_get_size = (xdo_get_window_size(xdo, foreground_window, &width_ret, &height_ret) == 0);

                    bool can_get_rect = (can_get_location && can_get_size);

                    bool is_fullscreen_window = (horizontal == width_ret) && (vertical == height_ret);
                    bool should_not_resize_screen = (!can_get_rect || is_fullscreen_window);

                    if (should_not_resize_screen) {
                        s_width = horizontal;
                        s_height = vertical;

                        letter_x = 0;
                        letter_y = 0;
                    }
                    else {
                        s_height = osu_y * 0.8;
                        s_width = s_height * 4 / 3;

                        long left = x_ret;
                        long top = y_ret;
                        long right = left + width_ret;
                        long bottom = top + height_ret;

                        letter_x = left + ((right - left) - s_width) / 2;
                        letter_y = top + osu_y * 0.117;
                    }
                }
                else {
                    s_height = osu_y * 0.8;
                    s_width = s_height * 4 / 3;

                    double l = (horizontal - osu_x) * (osu_h + 100) / 200.0;
                    double r = l + osu_x;
                    letter_x = l + ((r - l) - s_width) / 2;
                    letter_y = (vertical - osu_y) * (osu_v + 100) / 200.0 + osu_y * 0.117;
                }
            }
            else {
                s_width = horizontal;
                s_height = vertical;
                letter_x = 0;
                letter_y = 0;
            }
        }
        else {
            s_width = horizontal;
            s_height = vertical;
            letter_x = 0;
            letter_y = 0;
        }
    }
    else {
        s_width = horizontal;
        s_height = vertical;
        letter_x = 0;
        letter_y = 0;
    }

    double x = 0, y = 0;
    int px = 0, py = 0;

    if (xdo_get_mouse_location(xdo, &px, &py, NULL) == 0) {

        if (!is_letterbox) {
            letter_x = floor(1.0 * px / osu_x) * osu_x;
            letter_y = floor(1.0 * py / osu_y) * osu_y;
        }

        double fx = (1.0 * px - letter_x) / s_width;

        if (is_left_handed) {
            fx = 1 - fx;
        }

        double fy = (1.0 * py - letter_y) / s_height;

        fx = std::min(fx, 1.0);
        fx = std::max(fx, 0.0);

        fy = std::min(fy, 1.0);
        fy = std::max(fy, 0.0);

        x = -97 * fx + 44 * fy + 184;
        y = -76 * fx - 40 * fy + 324;
    }
#else
    // getting device resolution
    double letter_x, letter_y, s_height, s_width;

    HWND handle = GetForegroundWindow();
    if (handle) {
        TCHAR w_title[256];
        GetWindowText(handle, w_title, GetWindowTextLength(handle));
        std::string title = w_title;
        if (title.find("osu!") == 0) {
            RECT oblong;
            GetWindowRect(handle, &oblong);
            s_height = osu_y * 0.8;
            s_width = s_height * 4 / 3;
            if (!is_letterbox) {
                letter_x = oblong.left + ((oblong.right - oblong.left) - s_width) / 2;
                letter_y = oblong.top + osu_y * 0.117;
            } else {
                double l = (horizontal - osu_x) * (osu_h + 100) / 200.0;
                double r = l + osu_x;
                letter_x = l + ((r - l) - s_width) / 2;
                letter_y = (vertical - osu_y) * (osu_v + 100) / 200.0 + osu_y * 0.117;
            }
        } else {
            s_width = horizontal;
            s_height = vertical;
            letter_x = 0;
            letter_y = 0;
        }
    } else {
        s_width = horizontal;
        s_height = vertical;
        letter_x = 0;
        letter_y = 0;
    }
    double x, y;
    POINT point;
    if (GetCursorPos(&point)) {
        if (!is_letterbox) {
            letter_x = floor(1.0 * point.x / osu_x) * osu_x;
            letter_y = floor(1.0 * point.y / osu_y) * osu_y;
        }
        double fx = (1.0 * point.x - letter_x) / s_width;
        if (is_left_handed) {
            fx = 1 - fx;
        }
        double fy = (1.0 * point.y - letter_y) / s_height;
        fx = std::min(fx, 1.0);
        fx = std::max(fx, 0.0);
        fy = std::min(fy, 1.0);
        fy = std::max(fy, 0.0);
        x = -97 * fx + 44 * fy + 184;
        y = -76 * fx - 40 * fy + 324;
    }
#endif

    return std::make_pair(x, y);
}

void draw() {
    auto [x, y] = get_xy();

    window.draw(bg);

    // initializing pss and pss2 (kuvster's magic)
    int oof = 6;
    std::vector<double> pss = {211.0, 159.0};
    double dist = hypot(211 - x, 159 - y);
    double centreleft0 = 211 - 0.7237 * dist / 2;
    double centreleft1 = 159 + 0.69 * dist / 2;
    for (int i = 1; i < oof; i++) {
        std::vector<double> bez = {211, 159, centreleft0, centreleft1, x, y};
        auto [p0, p1] = input::bezier(1.0 * i / oof, bez, 6);
        pss.push_back(p0);
        pss.push_back(p1);
    }
    pss.push_back(x);
    pss.push_back(y);
    double a = y - centreleft1;
    double b = centreleft0 - x;
    double le = hypot(a, b);
    a = x + a / le * 60;
    b = y + b / le * 60;
    int a1 = 258;
    int a2 = 228;
    dist = hypot(a1 - a, a2 - b);
    double centreright0 = a1 - 0.6 * dist / 2;
    double centreright1 = a2 + 0.8 * dist / 2;
    int push = 20;
    double s = x - centreleft0;
    double t = y - centreleft1;
    le = hypot(s, t);
    s *= push / le;
    t *= push / le;
    double s2 = a - centreright0;
    double t2 = b - centreright1;
    le = hypot(s2, t2);
    s2 *= push / le;
    t2 *= push / le;
    for (int i = 1; i < oof; i++) {
        std::vector<double> bez = {x, y, x + s, y + t, a + s2, b + t2, a, b};
        auto [p0, p1] = input::bezier(1.0 * i / oof, bez, 8);
        pss.push_back(p0);
        pss.push_back(p1);
    }
    pss.push_back(a);
    pss.push_back(b);
    for (int i = oof - 1; i > 0; i--) {
        std::vector<double> bez = {1.0 * a1, 1.0 * a2, centreright0, centreright1, a, b};
        auto [p0, p1] = input::bezier(1.0 * i / oof, bez, 6);
        pss.push_back(p0);
        pss.push_back(p1);
    }
    pss.push_back(a1);
    pss.push_back(a2);
    double mpos0 = (a + x) / 2 - 52 - 15;
    double mpos1 = (b + y) / 2 - 34 + 5;
    double dx = -38;
    double dy = -50;

    const int iter = 25;

    std::vector<double> pss2 = {pss[0] + dx, pss[1] + dy};
    for (int i = 1; i < iter; i++) {
        auto [p0, p1] = input::bezier(1.0 * i / iter, pss, 38);
        pss2.push_back(p0 + dx);
        pss2.push_back(p1 + dy);
    }
    pss2.push_back(pss[36] + dx);
    pss2.push_back(pss[37] + dy);

    device.setPosition(mpos0 + dx + offset_x, mpos1 + dy + offset_y);

    // drawing mouse
    if (is_mouse) {
        window.draw(device);
    }

    // drawing arms
    sf::VertexArray fill(sf::TriangleStrip, 26);
    for (int i = 0; i < 26; i += 2) {
        fill[i].position = sf::Vector2f(pss2[i], pss2[i + 1]);
        fill[i + 1].position = sf::Vector2f(pss2[52 - i - 2], pss2[52 - i - 1]);
        fill[i].color = sf::Color(paw_r, paw_g, paw_b, paw_a);
        fill[i + 1].color = sf::Color(paw_r, paw_g, paw_b, paw_a);
    }
    window.draw(fill);

    // drawing first arm arc
    int shad = paw_edge_a / 3;
    sf::VertexArray edge(sf::TriangleStrip, 52);
    double width = 7;
    sf::CircleShape circ(width / 2);
    circ.setFillColor(sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, shad));
    circ.setPosition(pss2[0] - width / 2, pss2[1] - width / 2);
    window.draw(circ);
    for (int i = 0; i < 50; i += 2) {
        double vec0 = pss2[i] - pss2[i + 2];
        double vec1 = pss2[i + 1] - pss2[i + 3];
        double dist = hypot(vec0, vec1);
        edge[i].position = sf::Vector2f(pss2[i] + vec1 / dist * width / 2, pss2[i + 1] - vec0 / dist * width / 2);
        edge[i + 1].position = sf::Vector2f(pss2[i] - vec1 / dist * width / 2, pss2[i + 1] + vec0 / dist * width / 2);
        edge[i].color = sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, shad);
        edge[i + 1].color = sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, shad);
        width -= 0.08;
    }
    double vec0 = pss2[50] - pss2[48];
    double vec1 = pss2[51] - pss2[49];
    dist = hypot(vec0, vec1);
    edge[51].position = sf::Vector2f(pss2[50] + vec1 / dist * width / 2, pss2[51] - vec0 / dist * width / 2);
    edge[50].position = sf::Vector2f(pss2[50] - vec1 / dist * width / 2, pss2[51] + vec0 / dist * width / 2);
    edge[50].color = sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, shad);
    edge[51].color = sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, shad);
    window.draw(edge);
    circ.setRadius(width / 2);
    circ.setPosition(pss2[50] - width / 2, pss2[51] - width / 2);
    window.draw(circ);

    // drawing second arm arc
    sf::VertexArray edge2(sf::TriangleStrip, 52);
    width = 6;
    sf::CircleShape circ2(width / 2);
    circ2.setFillColor(sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, paw_edge_a));
    circ2.setPosition(pss2[0] - width / 2, pss2[1] - width / 2);
    window.draw(circ2);
    for (int i = 0; i < 50; i += 2) {
        vec0 = pss2[i] - pss2[i + 2];
        vec1 = pss2[i + 1] - pss2[i + 3];
        double dist = hypot(vec0, vec1);
        edge2[i].position = sf::Vector2f(pss2[i] + vec1 / dist * width / 2, pss2[i + 1] - vec0 / dist * width / 2);
        edge2[i + 1].position = sf::Vector2f(pss2[i] - vec1 / dist * width / 2, pss2[i + 1] + vec0 / dist * width / 2);
        edge2[i].color = sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, paw_edge_a);
        edge2[i + 1].color = sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, paw_edge_a);
        width -= 0.08;
    }
    vec0 = pss2[50] - pss2[48];
    vec1 = pss2[51] - pss2[49];
    dist = hypot(vec0, vec1);
    edge2[51].position = sf::Vector2f(pss2[50] + vec1 / dist * width / 2, pss2[51] - vec0 / dist * width / 2);
    edge2[50].position = sf::Vector2f(pss2[50] - vec1 / dist * width / 2, pss2[51] + vec0 / dist * width / 2);
    edge2[50].color = sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, paw_edge_a);
    edge2[51].color = sf::Color(paw_edge_r, paw_edge_g, paw_edge_b, paw_edge_a);
    window.draw(edge2);
    circ2.setRadius(width / 2);
    circ2.setPosition(pss2[50] - width / 2, pss2[51] - width / 2);
    window.draw(circ2);

    // drawing keypresses
    bool left_key = false;

    for (Json::Value &v : left_key_value) {
        if (input::is_pressed(v.asInt())) {
            left_key = true;
            break;
        }
    }

    if (left_key) {
        if (!left_key_state) {
            key_state = 1;
            left_key_state = true;
        }
    } else {
        left_key_state = false;
    }

    bool right_key = false;

    for (Json::Value &v : right_key_value) {
        if (input::is_pressed(v.asInt())) {
            right_key = true;
            break;
        }
    }

    if (right_key) {
        if (!right_key_state) {
            key_state = 2;
            right_key_state = true;
        }
    } else {
        right_key_state = false;
    }
    
    bool wave_key = false;

    for (Json::Value &v : wave_key_value) {
        if (input::is_pressed(v.asInt())) {
            wave_key = true;
            break;
        }
    }

    if (wave_key) {
        if (!wave_key_state) {
            key_state = 3;
            wave_key_state = true;
        }
    } else {
        wave_key_state = false;
    }

    if (!left_key_state && !right_key_state && !wave_key_state) {
        key_state = 0;
        window.draw(up);
    }

    if (key_state == 1) {
        if ((clock() - std::max(timer_right_key, timer_wave_key)) / CLOCKS_PER_SEC > BONGO_KEYPRESS_THRESHOLD) {
            if (!is_left_handed) {
                window.draw(left);
            } else {
                window.draw(right);
            }
            timer_left_key = clock();
        } else {
            window.draw(up);
        }
    } else if (key_state == 2) {
        if ((clock() - std::max(timer_left_key, timer_wave_key)) / CLOCKS_PER_SEC > BONGO_KEYPRESS_THRESHOLD) {
            if (!is_left_handed) {
                window.draw(right);
            } else {
                window.draw(left);
            }
            timer_right_key = clock();
        } else {
            window.draw(up);
        }
    } else if (key_state == 3) {
        if ((clock() - std::max(timer_left_key, timer_right_key)) / CLOCKS_PER_SEC > BONGO_KEYPRESS_THRESHOLD) {
            window.draw(wave);
            timer_wave_key = clock();
        } else {
            window.draw(up);
        }
    }

    // drawing tablet
    if (!is_mouse) {
        window.draw(device);
    }
    
    // draw smoke
    bool is_smoke_key_pressed = false;

    for (Json::Value &v : smoke_key_value) {
        if (input::is_pressed(v.asInt())) {
            is_smoke_key_pressed = true;
            break;
        }
    }

    if (is_enable_toggle_smoke) {
        previous_smoke_key_state = current_smoke_key_state;
        current_smoke_key_state = is_smoke_key_pressed;

        bool is_smoke_key_down = current_smoke_key_state && (current_smoke_key_state != previous_smoke_key_state);

        if (is_smoke_key_down) {
            is_toggle_smoke = !is_toggle_smoke;
        }
    }
    else {
        is_toggle_smoke = is_smoke_key_pressed;
    }

    if (is_toggle_smoke) {
        window.draw(smoke);
    }
}

void cleanup() {
#if defined(__unix__) || defined(__unix)
    delete xdo;
#endif
}
}; // namespace osu
