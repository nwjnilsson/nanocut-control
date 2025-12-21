#include "View.h"
#include "../NcRender/primitives/Primitive.h"
#include "../input/InputState.h"
#include "NcApp.h"
#include <algorithm>
#include <cmath>

View::View(NcApp* app) : m_app(app) {}

void View::adjustZoom(double zoom_factor, const Point2d& zoom_center)
{
  // Calculate new zoom level with bounds checking
  double new_zoom = m_zoom * zoom_factor;

  // Apply zoom limits (prevent infinite zoom in/out)
  const double min_zoom = 0.01;
  const double max_zoom = 100.0;
  new_zoom = std::max(min_zoom, std::min(max_zoom, new_zoom));

  // Zoom towards the specified center point
  if (zoom_center.x != 0 || zoom_center.y != 0) {
    // Calculate the zoom center in world coordinates before zoom change
    double world_x = (zoom_center.x - m_pan.x) / m_zoom;
    double world_y = (zoom_center.y - m_pan.y) / m_zoom;

    // Update zoom level
    m_zoom = new_zoom;

    // Calculate new pan to keep world point under the same screen position
    m_pan.x = zoom_center.x - world_x * m_zoom;
    m_pan.y = zoom_center.y - world_y * m_zoom;
  }
  else {
    m_zoom = new_zoom;
  }
}

void View::setMoveViewActive(bool active)
{
  m_move_view = active;
  if (active && m_app) {
    // Store current mouse position as start of drag
    m_last_mouse_pos = m_app->getInputState().getMousePosition();
  }
}

void View::handleMouseMove(const Point2d& current_mouse_pos)
{
  if (m_move_view) {
    // Calculate delta and update pan
    Point2d delta = { current_mouse_pos.x - m_last_mouse_pos.x,
                             current_mouse_pos.y - m_last_mouse_pos.y };
    adjustPan(delta);
    m_last_mouse_pos = current_mouse_pos;
  }
}

Point2d View::screenToMatrix(const Point2d& screen_pos) const
{
  return { (screen_pos.x - m_pan.x) / m_zoom,
           (screen_pos.y - m_pan.y) / m_zoom };
}

Point2d View::matrixToScreen(const Point2d& matrix_pos) const
{
  return { matrix_pos.x * m_zoom + m_pan.x, matrix_pos.y * m_zoom + m_pan.y };
}

void View::applyViewTransformation(Primitive* primitive)
{
  if (!primitive)
    return;

  if (primitive->data.is_null() ||
      !primitive->data.contains("original_offset")) {
    primitive->data["original_offset"] = { { "x", primitive->offset[0] },
                                           { "y", primitive->offset[1] },
                                           { "z", primitive->offset[2] } };
  }

  double orig_x = primitive->data["original_offset"]["x"].get<double>();
  double orig_y = primitive->data["original_offset"]["y"].get<double>();
  primitive->offset[0] = orig_x * m_zoom + m_pan.x;
  primitive->offset[1] = orig_y * m_zoom + m_pan.y;
  primitive->scale = m_zoom;
}

std::function<void(Primitive*)> View::getViewMatrixCallback()
{
  return [this](Primitive* primitive) { applyViewTransformation(primitive); };
}
