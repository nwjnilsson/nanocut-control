#include <string.h>
#include <chrono>
#include <sys/time.h>
#include <ctime>
#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ftw.h>

#include "EasyRender.h"
#include "logging/loguru.h"
#include "gui/imgui.h"
#include "gui/imgui_impl_glfw.h"
#include "gui/imgui_impl_opengl2.h"

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#include <GLFW/glfw3.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
   //define something for Windows (32-bit and 64-bit, this part is common)
   #include <GL/freeglut.h>
   #include <GL/gl.h>
   #define GL_CLAMP_TO_EDGE 0x812F
   #ifdef _WIN64
      //define something for Windows (64-bit only)
   #else
      //define something for Windows (32-bit only)
   #endif
#elif __APPLE__
    #include <OpenGL/glu.h>
#elif __linux__
    #include <GL/glu.h>
#elif __unix__
    #include <GL/glu.h>
#elif defined(_POSIX_VERSION)
    // POSIX
#else
#   error "Unknown compiler"
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

const auto EasyRenderProgramStartTime = std::chrono::steady_clock::now();

/*
    GLFW Key Callbacks
*/
void EasyRender::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    EasyRender *self = reinterpret_cast<EasyRender *>(glfwGetWindowUserPointer(window));
    if (self != NULL)
    {
        std::string keyname;
        if (glfwGetKeyName(key, scancode) != NULL)
        {
            keyname = std::string(glfwGetKeyName(key, scancode));
        }
        else
        {
            switch(key)
            {
                case 256: keyname = "Escape"; break;
                case 32: keyname = "Space"; break;
                case 258: keyname = "Tab"; break;
                case 265: keyname = "Up"; break;
                case 264: keyname = "Down"; break;
                case 263: keyname = "Left"; break;
                case 262: keyname = "Right"; break;
                case 266: keyname = "PgUp"; break;
                case 267: keyname = "PgDown"; break;
                case 340: keyname = "LeftShift"; break;
                case 347: keyname = "LeftControl"; break;
		case 341: keyname = "LeftControl"; break;
                case 343: keyname = "RightControl"; break;
		case 345: keyname = "RghtControl"; break;
                default: keyname = "None";
            }
            if (keyname == "None")
            {
                //LOG_F(INFO, "(key_callback) Unknown key: %d", key);
            }
        }
        if (!self->imgui_io->WantCaptureKeyboard || !self->imgui_io->WantCaptureMouse)
        {
            for (size_t x = 0; x < self->event_stack.size(); x++)
            {
                if (self->event_stack.at(x)->view == self->CurrentView)
                {
                    if (self->event_stack.at(x)->type == "keyup" && action == 1)
                    {
                        if (self->event_stack.at(x)->key == keyname)
                        {
                            if (self->event_stack.at(x)->callback != NULL)
                            {
                                self->event_stack.at(x)->callback({{"type", self->event_stack.at(x)->type}, {"key", keyname}, {"action", action}});
                            }
                        }
                    }
                    if (self->event_stack.at(x)->type == "keydown" && action == 0)
                    {
                        if (self->event_stack.at(x)->key == keyname)
                        {
                            if (self->event_stack.at(x)->callback != NULL)
                            {
                                self->event_stack.at(x)->callback({{"type", self->event_stack.at(x)->type}, {"key", keyname}, {"action", action}});
                            }
                        }
                    }
                    if (self->event_stack.at(x)->type == "repeat" && action == 2)
                    {
                        if (self->event_stack.at(x)->key == keyname)
                        {
                            if (self->event_stack.at(x)->callback != NULL)
                            {
                                self->event_stack.at(x)->callback({{"type", self->event_stack.at(x)->type}, {"key", keyname}, {"action", action}});
                            }
                        }
                    }
                }
            }
        }
    }
}
void EasyRender::mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    EasyRender *self = reinterpret_cast<EasyRender *>(glfwGetWindowUserPointer(window));
    if (self != NULL)
    {
        std::string event;
        if (button == 0) //Left click
        {
            if (action == 0) //Up
            {
                event = "left_click_up";
            }
            else if (action == 1) //Down
            {
                event = "left_click_down";
            }
        }
        else if (button == 1) //Right Click
        {
            if (action == 0) //Up
            {
                event = "right_click_up";
            }
            else if (action == 1) //Down
            {
                event = "right_click_down";
            }
        }
        else if (button == 2) //Middle Click
        {
            if (action == 0) //Up
            {
                event = "middle_click_up";
            }
            else if (action == 1) //Down
            {
                event = "middle_click_down";
            }
        }
        if (!self->imgui_io->WantCaptureKeyboard || !self->imgui_io->WantCaptureMouse)
        {
            for (size_t x = 0; x < self->event_stack.size(); x++)
            {
                if (self->event_stack.at(x)->view == self->CurrentView)
                {
                    if (self->event_stack.at(x)->type == event)
                    {
                        if (self->event_stack.at(x)->callback != NULL)
                        {
                            self->event_stack.at(x)->callback({{"event", event}});
                        }
                    }
                }
            }
            double_point_t m = self->GetWindowMousePosition();
            std::vector<PrimitiveContainer*>::iterator it;
            for ( it = self->Primitive_stack.end(); it != self->Primitive_stack.begin(); )
            {
                --it;
                if ((*it)->properties->view == self->CurrentView)
                {
                    if ((*it)->properties->visible == true && (*it)->properties->mouse_over == true)
                    {
                        if ((*it)->properties->mouse_callback != NULL)
                        {
                            double_point_t matrix_mouse = m;
                            matrix_mouse.x = (matrix_mouse.x - (*it)->properties->offset[0]) / (*it)->properties->scale;
                            matrix_mouse.y = (matrix_mouse.y - (*it)->properties->offset[1]) / (*it)->properties->scale;
                            (*it)->properties->mouse_callback((*it), {
                                {"event", event},
                                {"mouse_pos", {
                                    {"x", m.x},
                                    {"y", m.y}
                                }},
                                {"matrix_mouse_pos", {
                                    {"x", matrix_mouse.x},
                                    {"y", matrix_mouse.y}
                                }}
                            });
                            return;
                        }
                    }
                }
            }
        }
    }
}
void EasyRender::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    EasyRender *self = reinterpret_cast<EasyRender *>(glfwGetWindowUserPointer(window));
    if (self != NULL)
    {
        if (!self->imgui_io->WantCaptureKeyboard || !self->imgui_io->WantCaptureMouse)
        {
            for (size_t x = 0; x < self->event_stack.size(); x++)
            {
                if (self->event_stack.at(x)->view == self->CurrentView)
                {
                    if (yoffset > 0)
                    {
                        if (self->event_stack.at(x)->type == "scroll" && self->event_stack.at(x)->key == "up")
                        {
                            self->event_stack.at(x)->callback({{"scroll", yoffset}});
                        }
                    }
                    else
                    {
                        if (self->event_stack.at(x)->type == "scroll" && self->event_stack.at(x)->key == "down")
                        {
                            self->event_stack.at(x)->callback({{"scroll", yoffset}});
                        }
                    }
                }
            }
        }
    }
}
void EasyRender::cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    EasyRender *self = reinterpret_cast<EasyRender *>(glfwGetWindowUserPointer(window));
    if (self != NULL)
    {
        double_point_t m = self->GetWindowMousePosition();
        for (size_t x = 0; x < self->event_stack.size(); x++)
        {
            if (self->event_stack.at(x)->view == self->CurrentView)
            {
                if (self->event_stack.at(x)->type == "mouse_move")
                {
                    self->event_stack.at(x)->callback({{"event", "mouse_move"}, {"pos",{{"x", m.x},{"y", m.y}}}});
                }
            }
        }
    }
}
void EasyRender::window_size_callback(GLFWwindow* window, int width, int height)
{
    EasyRender *self = reinterpret_cast<EasyRender *>(glfwGetWindowUserPointer(window));
    if (self != NULL)
    {
        for (size_t x = 0; x < self->event_stack.size(); x++)
        {
            if (self->event_stack.at(x)->view == self->CurrentView)
            {
                if (self->event_stack.at(x)->type == "window_resize")
                {
                    self->event_stack.at(x)->callback({{"size",{{"width", width},{"height", height}}}});
                }
            }
        }
    }
}
/*
    Each Primitive type must have a method
*/
EasyPrimitive::Line* EasyRender::PushPrimitive(EasyPrimitive::Line* l)
{  
    PrimitiveContainer *c = new PrimitiveContainer(l);
    c->properties->view = this->CurrentView;
    Primitive_stack.push_back(c);
    return c->line;
}
EasyPrimitive::Text* EasyRender::PushPrimitive(EasyPrimitive::Text* t)
{  
    PrimitiveContainer *c = new PrimitiveContainer(t);
    c->properties->view = this->CurrentView;
    Primitive_stack.push_back(c);
    return c->text;
}
EasyPrimitive::Image* EasyRender::PushPrimitive(EasyPrimitive::Image* i)
{  
    PrimitiveContainer *c = new PrimitiveContainer(i);
    c->properties->view = this->CurrentView;
    Primitive_stack.push_back(c);
    return c->image;
}
EasyPrimitive::Path* EasyRender::PushPrimitive(EasyPrimitive::Path* p)
{  
    PrimitiveContainer *c = new PrimitiveContainer(p);
    c->properties->view = this->CurrentView;
    Primitive_stack.push_back(c);
    return c->path;
}
EasyPrimitive::Part* EasyRender::PushPrimitive(EasyPrimitive::Part* p)
{  
    PrimitiveContainer *c = new PrimitiveContainer(p);
    c->properties->view = this->CurrentView;
    Primitive_stack.push_back(c);
    return c->part;
}
EasyPrimitive::Arc* EasyRender::PushPrimitive(EasyPrimitive::Arc* a)
{  
    PrimitiveContainer *c = new PrimitiveContainer(a);
    c->properties->view = this->CurrentView;
    Primitive_stack.push_back(c);
    return c->arc;
}
EasyPrimitive::Circle* EasyRender::PushPrimitive(EasyPrimitive::Circle* ci)
{  
    PrimitiveContainer *c = new PrimitiveContainer(ci);
    c->properties->view = this->CurrentView;
    Primitive_stack.push_back(c);
    return c->circle;
}
EasyPrimitive::Box* EasyRender::PushPrimitive(EasyPrimitive::Box* b)
{  
    PrimitiveContainer *c = new PrimitiveContainer(b);
    c->properties->view = this->CurrentView;
    Primitive_stack.push_back(c);
    return c->box;
}
void EasyRender::PushTimer(unsigned long interval, bool (*c)())
{
    EasyRenderTimer *t = new EasyRenderTimer;
    t->interval = interval;
    t->timestamp = this->Millis();
    t->callback = c;
    t->view = this->CurrentView;
    t->self_pointer = NULL;
    t->callback_with_self = NULL;
    timer_stack.push_back(t);
}
void EasyRender::PushTimer(unsigned long interval, bool (*c)(void*), void *s)
{
    EasyRenderTimer *t = new EasyRenderTimer;
    t->interval = interval;
    t->timestamp = this->Millis();
    t->callback = NULL;
    t->view = this->CurrentView;
    t->self_pointer = s;
    t->callback_with_self = c;
    timer_stack.push_back(t);
}
EasyRender::EasyRenderGui *EasyRender::PushGui(bool v, void (*c)())
{
    EasyRender::EasyRenderGui *g = new EasyRender::EasyRenderGui;
    g->visible = v;
    g->callback = c;
    g->view = this->CurrentView;
    g->self_pointer = NULL;
    g->callback_with_self = NULL;
    gui_stack.push_back(g);
    return g;
}
EasyRender::EasyRenderGui *EasyRender::PushGui(bool v, void (*c)(void *p), void *s)
{
    EasyRender::EasyRenderGui *g = new EasyRender::EasyRenderGui;
    g->visible = v;
    g->callback = NULL;
    g->view = this->CurrentView;
    g->self_pointer = s;
    g->callback_with_self = c;
    gui_stack.push_back(g);
    return g;
}
void EasyRender::PushEvent(std::string key, std::string type, void (*callback)(nlohmann::json))
{
    EasyRenderEvent *e = new EasyRenderEvent;
    e->key = key;
    e->type = type;
    e->callback = callback;
    e->view = this->CurrentView;
    event_stack.push_back(e);
    if (type == "window_resize" && this->Window != NULL)
    {
        double_point_t size = this->GetWindowSize();
        this->WindowSize[0] = (int)size.x;
        this->WindowSize[1] = (int)size.y;
        EasyRender::window_size_callback(this->Window, this->WindowSize[0], this->WindowSize[1]);
    }
}
void EasyRender::SetWindowTitle(std::string w)
{
    this->WindowTitle = w;
}
void EasyRender::SetWindowSize(int width, int height)
{
    this->WindowSize[0] = width;
    this->WindowSize[1] = height;
}
void EasyRender::SetShowCursor(bool s)
{
    this->ShowCursor = s;
}
void EasyRender::SetAutoMaximize(bool m)
{
    this->AutoMaximize = m;
}
void EasyRender::SetGuiIniFileName(std::string i)
{
    this->GuiIniFileName = (char*)malloc(sizeof(char) * strlen(i.c_str()));
    strcpy(this->GuiIniFileName , i.c_str());
}
void EasyRender::SetGuiLogFileName(std::string l)
{
    this->GuiLogFileName = (char*)malloc(sizeof(char) * strlen(l.c_str()));
    strcpy(this->GuiLogFileName , l.c_str());
}
void EasyRender::SetMainLogFileName(std::string l)
{
    this->MainLogFileName = l;
}
void EasyRender::SetGuiStyle(std::string s)
{
    this->GuiStyle = s;
}
void EasyRender::SetClearColor(float r, float g, float b)
{
    this->ClearColor[0] = r / 255;
    this->ClearColor[1] = g / 255;
    this->ClearColor[2] = b / 255;
    //LOG_F(INFO, "Set Clear Color (%.4f, %.4f, %.4f)", this->ClearColor[0], this->ClearColor[1], this->ClearColor[2]);
}
void EasyRender::SetShowFPS(bool show_fps)
{
    this->ShowFPS = show_fps;
}
void EasyRender::SetColorByName(float *c, std::string color)
{
    if (color == "white")
    {
        c[0] = 255;
        c[1] = 255;
        c[2] = 255;
        c[3] = 255;
    }
    else if (color == "black")
    {
        c[0] = 0;
        c[1] = 0;
        c[2] = 0;
        c[3] = 255;
    }
    else if (color == "grey")
    {
        c[0] = 100;
        c[1] = 100;
        c[2] = 100;
        c[3] = 255;
    }
    else if (color == "light-red")
    {
        c[0] = 190;
        c[1] = 0;
        c[2] = 0;
        c[3] = 255;
    }
    else if (color == "red")
    {
        c[0] = 255;
        c[1] = 0;
        c[2] = 0;
        c[3] = 255;
    }
    else if (color == "light-green")
    {
        c[0] = 0;
        c[1] = 190;
        c[2] = 0;
        c[3] = 255;
    }
    else if (color == "green")
    {
        c[0] = 0;
        c[1] = 255;
        c[2] = 0;
        c[3] = 255;
    }
    else if (color == "light-blue")
    {
        c[0] = 0;
        c[1] = 0;
        c[2] = 190;
        c[3] = 255;
    }
    else if (color == "blue")
    {
        c[0] = 0;
        c[1] = 0;
        c[2] = 255;
        c[3] = 255;
    }
    else if (color == "yellow")
    {
        c[0] = 255;
        c[1] = 255;
        c[2] = 0;
        c[3] = 255;
    }
    else
    {
        LOG_F(WARNING, "(EasyRender::SetColorByName) Color \"%s\" not found, defaulting to white", color.c_str());
        c[0] = 255;
        c[1] = 255;
        c[2] = 255;
        c[3] = 255;
    }
}
void EasyRender::SetCurrentView(std::string v)
{
    this->CurrentView = v;
}
unsigned long EasyRender::Millis()
{
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::seconds;
    using std::chrono::system_clock;
    auto end = std::chrono::steady_clock::now();
    unsigned long m = (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(end - EasyRenderProgramStartTime).count();
    return m;
}
void EasyRender::Delay(unsigned long ms)
{
  unsigned long delay_timer = EasyRender::Millis();
  while((EasyRender::Millis() - delay_timer) < ms);
  return;
}
std::string EasyRender::GetEvironmentVariable(const std::string & var)
{
    const char * val = std::getenv( var.c_str() );
    if ( val == nullptr )
    {
        return "";
    }
    else
    {
        return val;
    }
}
std::string EasyRender::GetConfigDirectory()
{
    struct stat info;
    #ifdef __linux__
        std::string path = this->GetEvironmentVariable("HOME") + "/.config/" + this->WindowTitle + "/";
        if(stat(path.c_str(), &info ) != 0)
        {
            LOG_F(INFO, "(EasyRender::GetConfigDirectory) %s does not exist, creating it!", path.c_str());
            mkdir(path.c_str(),(mode_t)0733);
        }
        return path;
    #elif __APPLE__
        std::string path = this->GetEvironmentVariable("HOME") + "/Library/" + this->WindowTitle + "/";
        if(stat(path.c_str(), &info ) != 0)
        {
            LOG_F(INFO, "(EasyRender::GetConfigDirectory) %s does not exist, creating it!", path.c_str());
            mkdir(path.c_str(),(mode_t)0733);
        }
        return path;
    #elif _WIN32
        std::string path = this->GetEvironmentVariable("APPDATA") + "\\" + this->WindowTitle + "\\";
        if(stat(path.c_str(), &info ) != 0)
        {
            LOG_F(INFO, "(EasyRender::GetConfigDirectory) %s does not exist, creating it!", path.c_str());
            mkdir(path.c_str());
        }
        return path;
    #else     
        #error Platform not supported! Must be Linux, OSX, or Windows
        return "";
    #endif
}
double_point_t EasyRender::GetWindowMousePosition()
{
    double mouseX, mouseY;
    glfwGetCursorPos(this->Window, &mouseX, &mouseY);
    mouseX -= 2;
    mouseY -= 4;
    return {mouseX - (this->WindowSize[0]/ 2.0f), (this->WindowSize[1] - mouseY) - (this->WindowSize[1] / 2.0f)};
}
double_point_t EasyRender::GetWindowSize()
{
    glfwGetFramebufferSize(this->Window, &this->WindowSize[0], &this->WindowSize[1]);
    return {(double)this->WindowSize[0], (double)this->WindowSize[1]};
}
uint8_t EasyRender::GetFramesPerSecond()
{
    return (uint8_t)(1000.0f / (float)RenderPerformance);
}
std::string EasyRender::GetCurrentView()
{
    return this->CurrentView;
}
std::vector<PrimitiveContainer *> *EasyRender::GetPrimitiveStack()
{
    return &this->Primitive_stack;
}
nlohmann::json EasyRender::DumpPrimitiveStack()
{
    nlohmann::json r;
    std::vector<PrimitiveContainer*>::iterator it;
    for ( it = this->Primitive_stack.begin(); it != this->Primitive_stack.end(); )
    {
        r.push_back((*it)->serialize());
        ++it;
    }
    return r;
}
nlohmann::json EasyRender::ParseJsonFromFile(std::string filename)
{
    std::ifstream json_file(filename);
    if (json_file.is_open())
    {
        std::string json_string((std::istreambuf_iterator<char>(json_file)), std::istreambuf_iterator<char>());
        return nlohmann::json::parse(json_string.c_str());
    }
    else
    {
        return NULL;
    }
}
void EasyRender::DumpJsonToFile(std::string filename, nlohmann::json j)
{
    try
    {
        std::ofstream out(filename);
        out << j.dump(4);
        out.close();
    }
    catch(const std::exception& e)
    {
        LOG_F(ERROR, "(EasyRender::DumpJsonToFile) %s", e.what());
    }
}
std::string EasyRender::FileToString(std::string filename)
{
    std::ifstream f(filename);
    if (f.is_open())
    {
        std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        return s;
    }
    else
    {
        return "";
    }
}
void EasyRender::StringToFile(std::string filename, std::string s)
{
    try
    {
        std::ofstream out(filename);
        out << s;
        out.close();
    }
    catch(const std::exception& e)
    {
        LOG_F(ERROR, "(EasyRender::StringToFile) %s", e.what());
    }
}
void EasyRender::DeletePrimitivesById(std::string id)
{
    std::vector<PrimitiveContainer*>::iterator it;
    for ( it = this->Primitive_stack.begin(); it != this->Primitive_stack.end(); )
    {
        if((*it)->properties->id == id)
        {
            (*it)->destroy();
            delete * it;  
            it = this->Primitive_stack.erase(it);
        }
        else 
        {
            ++it;
        }
    }
}
bool EasyRender::Init(int argc, char** argv)
{
    loguru::init(argc, argv);
    loguru::g_stderr_verbosity = 1;
    loguru::add_file(this->MainLogFileName.c_str(), loguru::Append, loguru::Verbosity_MAX);
    if (!glfwInit())
    {
        LOG_F(ERROR, "Could not init glfw window!");
        return false;
    }
    this->Window = glfwCreateWindow(this->WindowSize[0], this->WindowSize[1], this->WindowTitle.c_str(), NULL, NULL);
    if (this->Window == NULL)
    {
        LOG_F(ERROR, "Could not open GLFW Window!");
        return false;
    }
    if (this->AutoMaximize == true)
    {
        glfwMaximizeWindow(this->Window);
    }
    if (this->ShowCursor == false)
    {
        glfwSetInputMode(this->Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    glfwSetWindowUserPointer(this->Window, reinterpret_cast<void *>(this));
    glfwSetKeyCallback(this->Window, this->key_callback);
    glfwSetMouseButtonCallback(this->Window, this->mouse_button_callback);
    glfwSetScrollCallback(this->Window, this->scroll_callback);
    //glfwSetWindowCloseCallback(this->Window, window_close_callback);
    glfwSetCursorPosCallback(this->Window, this->cursor_position_callback);
    glfwSetWindowSizeCallback(this->Window, this->window_size_callback);
    glfwMakeContextCurrent(this->Window);
    glfwSwapInterval(1); // Enable vsync
    //Set calbacks here
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    this->imgui_io = &io;
    this->imgui_io->IniFilename = this->GuiIniFileName;
    this->imgui_io->LogFilename = this->GuiLogFileName;
    this->imgui_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    if (this->GuiStyle == "light")
    {
        ImGui::StyleColorsLight();
    }
    else if (this->GuiStyle == "dark")
    {
        ImGui::StyleColorsDark();
    }
    ImGui_ImplGlfw_InitForOpenGL(this->Window, true);
    ImGui_ImplOpenGL2_Init();
    return true;
}
bool EasyRender::Poll(bool should_quit)
{
    unsigned long begin_timestamp = this->Millis();
    glfwGetFramebufferSize(this->Window, &this->WindowSize[0], &this->WindowSize[1]);
    float ratio = this->WindowSize[0] / (float)this->WindowSize[1];
    double_point_t window_mouse_pos = this->GetWindowMousePosition();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthRange(0, 1);
    glDepthFunc(GL_LEQUAL);
    glClearColor(this->ClearColor[0], this->ClearColor[1], this->ClearColor[2], 255);
    /* IMGUI */
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    for (size_t x = 0; x < this->gui_stack.size(); x++)
    {
        if (this->gui_stack[x]->visible == true && this->gui_stack[x]->view == this->CurrentView)
        {
            if (this->gui_stack[x]->self_pointer == NULL)
            {
                if (this->gui_stack[x]->callback != NULL)
                {
                    this->gui_stack[x]->callback();
                }
            }
            else
            {
                if (this->gui_stack[x]->callback_with_self != NULL)
                {
                    this->gui_stack[x]->callback_with_self(this->gui_stack[x]->self_pointer);
                }
            }
        }
    }
    ImGui::Render();
    /*********/
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho( -this->WindowSize[0]/2, this->WindowSize[0]/2, -this->WindowSize[1]/2, this->WindowSize[1]/2, -1,1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0, 0, this->WindowSize[0], this->WindowSize[1]);
    glClear(GL_COLOR_BUFFER_BIT);
    sort(Primitive_stack.begin(), Primitive_stack.end(), [](auto* lhs, auto* rhs) {
        return lhs->properties->zindex < rhs->properties->zindex;
    });
    bool ignore_next_mouse_events = false;
    for (size_t x = 0; x < this->Primitive_stack.size(); x ++)
    {
        if (this->Primitive_stack[x]->properties->visible == true && this->Primitive_stack[x]->properties->view == this->CurrentView)
        {
            Primitive_stack[x]->render();
            if ((!this->imgui_io->WantCaptureKeyboard || !this->imgui_io->WantCaptureMouse) && ignore_next_mouse_events == false)
            {
                Primitive_stack[x]->process_mouse(window_mouse_pos.x, window_mouse_pos.y);
            }
        }
    }
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    glfwMakeContextCurrent(this->Window);
    glfwSwapBuffers(this->Window);
    glfwPollEvents();
    for (size_t x = 0; x < this->timer_stack.size(); x++)
    {
        if (this->timer_stack.at(x)->view == this->CurrentView)
        {
            if ((this->Millis() - this->timer_stack.at(x)->timestamp) > this->timer_stack.at(x)->interval)
            {
                this->timer_stack[x]->timestamp = this->Millis();
                if (this->timer_stack[x]->self_pointer == NULL)
                {
                    if (this->timer_stack[x]->callback != NULL)
                    {
                        if (this->timer_stack[x]->callback() == false) //Don't repeat
                        {
                            delete this->timer_stack[x];
                            this->timer_stack.erase(this->timer_stack.begin()+x);
                        }
                    }
                }
                else
                {
                    if (this->timer_stack[x]->callback_with_self != NULL)
                    {
                        if (this->timer_stack[x]->callback_with_self(this->timer_stack[x]->self_pointer) == false) //Don't repeat
                        {
                            delete this->timer_stack[x];
                            this->timer_stack.erase(this->timer_stack.begin()+x);
                        }
                    }
                }
            }
        }
    }
    this->RenderPerformance = (this->Millis() - begin_timestamp);
    if (this->ShowFPS == true)
    {
        if (this->FPS_Label == NULL)
        {
            LOG_F(INFO, "Creating FPS label!");
            this->FPS_Label = this->PushPrimitive(new EasyPrimitive::Text({0, 0}, "0", 30));
            this->FPS_Label->properties->visible = false;
            this->FPS_Label->properties->id = "FPS";
            this->SetColorByName(this->FPS_Label->properties->color, "white");
        } 
        else
        {
            this->FPS_Average.push_back(this->GetFramesPerSecond());
            if (this->FPS_Average.size() > 10)
            {
                float avg = 0;
                for (size_t x = 0; x < this->FPS_Average.size(); x++)
                {
                    avg += this->FPS_Average[x];
                }
                avg = avg / this->FPS_Average.size();
                this->FPS_Label->textval = std::to_string((int)avg);
                this->FPS_Label->properties->visible = true;
                this->FPS_Label->position.x = -(this->WindowSize[0] / 2.0f) + 10;
                this->FPS_Label->position.y = -(this->WindowSize[1] / 2.0f) + 10;
                this->FPS_Average.erase(this->FPS_Average.begin());
            }
        }
    }
    if (should_quit == true) return false;
    return !glfwWindowShouldClose(this->Window);
}
void EasyRender::Close()
{
    for (size_t x = 0; x < this->Primitive_stack.size(); x++)
    {
        this->Primitive_stack.at(x)->destroy();
        delete this->Primitive_stack.at(x);
    }
    for (size_t x = 0; x < this->timer_stack.size(); x++)
    {
        delete this->timer_stack.at(x);
        this->timer_stack.erase(this->timer_stack.begin()+x);
    }
    for (size_t x = 0; x < this->gui_stack.size(); x++)
    {
        delete this->gui_stack.at(x);
        this->gui_stack.erase(this->gui_stack.begin()+x);
    }
    for (size_t x = 0; x < this->event_stack.size(); x++)
    {
        delete this->event_stack.at(x);
        this->event_stack.erase(this->event_stack.begin()+x);
    }
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(this->Window);
    glfwTerminate();
    free(this->GuiIniFileName);
    free(this->GuiLogFileName);
}
