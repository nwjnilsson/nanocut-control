#ifndef CIRCLE_
#define CIRCLE_

#include "../PrimitiveProperties.h"
#include "../../json/json.h"
#include "../../geometry/geometry.h"
#include <string>

class EasyPrimitive::Circle{
    public:
        nlohmann::json mouse_event;
        PrimitiveProperties *properties;
        
        double_point_t center;
        float radius;
        float width;
        std::string style; //solid, dashed

        Circle(double_point_t c, float r)
        {
            this->properties = new PrimitiveProperties();
            this->center = c;
            this->radius = r;
            this->width = 1;
            this->style = "solid";
            this->mouse_event = NULL;
        }
        std::string get_type_name();
        void process_mouse(float mpos_x, float mpos_y);
        void render_arc(double cx, double cy, double radius, double start_angle, double end_angle);
        void render();
        void destroy();
        nlohmann::json serialize();
};

#endif //CIRCLE_