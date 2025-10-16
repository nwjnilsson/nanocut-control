#ifndef JET_CAM_VIEW_
#define JET_CAM_VIEW_

#include <application.h>
#include <dxf/dxflib/dl_dxf.h>
#include "PolyNest/PolyNest.h"

class DXFParsePathAdaptor;

class jetCamView{
    private:
        struct duplicate_part_t{
            bool visible;
        };
        struct preferences_data_t{
            float background_color[3] = { 0.0f, 0.0f, 0.0f };
        };
        struct job_options_data_t{
            float material_size[2] = { DEFAULT_MATERIAL_SIZE, DEFAULT_MATERIAL_SIZE };
            int origin_corner = 2;
        };
        struct tool_data_t{
            char tool_name[1024];
            std::unordered_map<std::string, float> params {
                {"pierce_height", 1.f},
                {"pierce_delay", 1.f},
                {"cut_height", 1.f},
                {"kerf_width", 1.f},
                {"feed_rate", 1.f},
                {"athc", 0.f}
            };
        };
        enum operation_types{
            jet_cutting,
        };
        struct toolpath_operation_t{
            bool enabled;
            bool last_enabled = false;
            int operation_type;
            int tool_number;
            double lead_in_length = -1;
            double lead_out_length = -1;
            std::string layer;
        };
        struct action_t{
            nlohmann::json data;
            std::vector<PrimitiveContainer*> parts;
            std::string action_id;
        };
        std::vector<action_t> action_stack;
        EasyRender::EasyRenderGui *menu_bar;
        static void ZoomEventCallback(nlohmann::json e);
        static void ViewMatrixCallback(PrimitiveContainer *p);
        static void MouseCallback(nlohmann::json e);
        static void MouseEventCallback(PrimitiveContainer* c,nlohmann::json e);
        static void KeyCallback(nlohmann::json e);
        static void RenderUI(void *p);

        double_point_t show_viewer_context_menu;
        double_point_t last_mouse_click_position;
        bool left_click_state;
        bool tab_state;
        #define JETCAM_TOOL_CONTOUR 0
        #define JETCAM_TOOL_NESTING 1
        #define JETCAM_TOOL_POINT 2
        int CurrentTool;
        FILE *dxf_fp;
        DL_Dxf *dl_dxf;
        PolyNest::PolyNest dxf_nest;
        DXFParsePathAdaptor *DXFcreationInterface;
        bool DxfFileOpen(std::string filename, std::string name);
        static bool DxfFileParseTimer(void *p);

        float ProgressWindowProgress;
        EasyRender::EasyRenderGui *ProgressWindowHandle;
        void ShowProgressWindow(bool v);
        static void RenderProgressWindow(void *p);
        
    public:
        PrimitiveContainer* mouse_over_part;
        PrimitiveContainer* edit_contour_part;
        size_t mouse_over_path;
        size_t edit_contour_path;

        std::string mouse_over_color;
        std::string outside_contour_color;
        std::string inside_contour_color;
        std::string open_contour_color;

        preferences_data_t preferences;
        job_options_data_t job_options;
        std::vector<tool_data_t> tool_library;
        std::vector<toolpath_operation_t> toolpath_operations;
        EasyPrimitive::Box *material_plane;

        jetCamView(){
            dxf_fp = NULL;
        };
        void DeletePart(std::string part_name);
        void PreInit();
        void Init();
        void Tick();
        void MakeActive();
        void Close();
};

#endif
