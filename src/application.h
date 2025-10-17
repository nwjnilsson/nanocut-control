#ifndef APPLICATION_
#define APPLICATION_

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
#include <EasyRender/EasyRender.h>
#include <../include/config.h>
#include <limits>

#define DEFAULT_UNIT          SCALE(1.f)

constexpr int INT_MIN { std::numeric_limits<int>::min() };
constexpr int INT_MAX { std::numeric_limits<int>::max() };

/*
    Primary structure to store global variables in
*/

class ncControlView;
class jetCamView;
class adminView;
class WebsocketClient;

struct global_variables_t{
    bool quit;
    double zoom;
    double_point_t pan;
    bool move_view;
    double_point_t mouse_pos_in_screen_coordinates;
    double_point_t mouse_pos_in_matrix_coordinates;
    unsigned long start_timestamp;
    EasyRender *renderer;
    WebsocketClient *websocket;
    /*  Define view classes here */
    ncControlView *nc_control_view;
    jetCamView *jet_cam_view;
    adminView *admin_view;

    
};
extern global_variables_t *globals;

#endif //APPLICATION_
