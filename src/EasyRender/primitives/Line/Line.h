#ifndef LINE_
#define LINE_

#include "../PrimitiveProperties.h"
#include "../../json/json.h"
#include "../../geometry/geometry.h"
#include <string>

class EasyPrimitive::Line{
    public:
        nlohmann::json mouse_event;
        PrimitiveProperties *properties;
        
        double_point_t start;
        double_point_t end;
        float width;
        std::string style; //solid, dashed

        Line(double_point_t s, double_point_t e)
        {
            this->properties = new PrimitiveProperties();
            this->start = s;
            this->end = e;
            this->width = 1;
            this->style = "solid";
            this->mouse_event = NULL;
        }
        std::string get_type_name();
        void process_mouse(float mpos_x, float mpos_y);
        void render();
        void destroy();
        nlohmann::json serialize();
};

#endif //LINE_