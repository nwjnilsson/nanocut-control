#ifndef ADMIN_VIEW_
#define ADMIN_VIEW_

// System includes
#include <NcRender/NcRender.h>
#include <application/View.h>

// Forward declarations
class NcApp;
class InputState;
struct ScrollEvent;

class NcAdminView : public View {
public:
  // Constructors and destructors
  explicit NcAdminView(NcApp* app)
    : View(app), ui(nullptr), m_app(app) {
        // Dependency injection constructor
      };

  // Move semantics (non-copyable due to resource management)
  NcAdminView(const NcAdminView&) = delete;
  NcAdminView& operator=(const NcAdminView&) = delete;
  NcAdminView(NcAdminView&&) = default;
  NcAdminView& operator=(NcAdminView&&) = default;

  // Public interface
  void preInit();
  void init();
  void tick() override;
  void makeActive();
  void close();

  void handleScrollEvent(const ScrollEvent& e,
                         const InputState&  input) override;

private:
  // Instance callbacks
  void zoomEventCallback(const ScrollEvent& e, const InputState& input);
  void renderUI();

  // Member variables
  NcRender::NcRenderGui* ui;
  NcApp* m_app{ nullptr }; // Application context dependency (injected)
};

#endif
