#ifndef APPLICATION_
#define APPLICATION_

#include <../include/config.h>
#include <EasyRender/geometry/geometry.h>
#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <ftw.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#define DEFAULT_UNIT SCALE(1.f)

/*
    Primary structure to store global variables in
*/

class ncControlView;
class jetCamView;
class adminView;
class WebsocketClient;
class EasyRender;

struct global_variables_t {
  bool             quit;
  double           zoom;
  double_point_t   pan;
  bool             move_view;
  double_point_t   mouse_pos_in_screen_coordinates;
  double_point_t   mouse_pos_in_matrix_coordinates;
  unsigned long    start_timestamp;
  EasyRender*      renderer;
  WebsocketClient* websocket;
  /*  Define view classes here */
  ncControlView* nc_control_view;
  jetCamView*    jet_cam_view;
  adminView*     admin_view;
};
extern global_variables_t* globals;

#endif // APPLICATION_
