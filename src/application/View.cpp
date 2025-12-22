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

void View::applyViewTransform(Primitive* primitive)
{
  if (!primitive)
    return;

  // Get view-specific work coordinate offset
  Point2d work_offset = getWorkOffset();

  // Calculate work coordinate offset transform values
  // This combines pan with work offset scaled by zoom
  const double wco_x = m_pan.x + std::abs(work_offset.x * m_zoom);
  const double wco_y = m_pan.y + std::abs(work_offset.y * m_zoom);

  // Prepare transform data
  TransformData transform;
  transform.zoom = m_zoom;
  transform.pan_x = m_pan.x;
  transform.pan_y = m_pan.y;
  transform.wco_x = wco_x;
  transform.wco_y = wco_y;

  // Apply transform using polymorphic dispatch
  primitive->applyTransform(transform);
}

std::function<void(Primitive*)> View::getTransformCallback()
{
  return [this](Primitive* primitive) { applyViewTransform(primitive); };
}
