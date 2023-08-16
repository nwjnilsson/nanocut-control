#ifndef Primitive_CONTAINTER_
#define Primitive_CONTAINTER_

#include <string>
#include <vector>
#include <chrono>
#include <sys/time.h>
#include <ctime>
#include <algorithm> 
#include "PrimitiveProperties.h"
#include "Line/Line.h"
#include "Text/Text.h"
#include "Image/Image.h"
#include "Path/Path.h"
#include "Arc/Arc.h"
#include "Circle/Circle.h"
#include "Box/Box.h"
#include "Part/Part.h"

class PrimitiveContainer{
    public:
        PrimitiveProperties *properties;
        std::string type;
        EasyPrimitive::Line *line;
        EasyPrimitive::Text *text;
        EasyPrimitive::Image *image;
        EasyPrimitive::Path *path;
        EasyPrimitive::Arc *arc;
        EasyPrimitive::Circle *circle;
        EasyPrimitive::Box *box;
        EasyPrimitive::Part *part;

        void process_mouse(float mpos_x, float mpos_y);
        void render();
        void destroy();
        nlohmann::json serialize();

        PrimitiveContainer(EasyPrimitive::Line *l)
        {
            line = l;
            this->type = line->get_type_name();
            properties = line->properties;
        }
        PrimitiveContainer(EasyPrimitive::Text *t)
        {
            text = t;
            this->type = text->get_type_name();
            properties = text->properties;
            this->text->render(); //Make sure height and width are calculated right away
        }
        PrimitiveContainer(EasyPrimitive::Image *i)
        {
            image = i;
            this->type = image->get_type_name();
            properties = image->properties;
        }
        PrimitiveContainer(EasyPrimitive::Path *p)
        {
            path = p;
            this->type = path->get_type_name();
            properties = path->properties;
        }
        PrimitiveContainer(EasyPrimitive::Part *p)
        {
            part = p;
            this->type = part->get_type_name();
            properties = part->properties;
        }
        PrimitiveContainer(EasyPrimitive::Arc *a)
        {
            arc = a;
            this->type = arc->get_type_name();
            properties = arc->properties;
        }
        PrimitiveContainer(EasyPrimitive::Circle *c)
        {
            circle = c;
            this->type = circle->get_type_name();
            properties = circle->properties;
        }
        PrimitiveContainer(EasyPrimitive::Box *b)
        {
            box = b;
            this->type = box->get_type_name();
            properties = box->properties;
        }
};

#endif //Primitive_CONTAINTER_