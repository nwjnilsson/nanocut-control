/**
 * @file PathImportCommon.h
 *
 * Shared, format-agnostic tail of the geometry importers. Both the DXF importer
 * (DXFParsePathAdaptor) and the SVG importer (SvgParsePathAdaptor) reduce their
 * source files to a set of ordered contours; everything after that -- global
 * bounding box, inside/outside (even-odd) contour classification, theme-color
 * assignment and pushing the finished Part into the renderer -- is identical.
 * That common logic lives here so the two importers stay behaviourally in sync.
 */

#ifndef PathImportCommon_
#define PathImportCommon_

#include <NcRender/NcRender.h>
#include <functional>
#include <string>
#include <vector>

class NcCamView;

namespace path_import {

/**
 * Build a Part primitive from a set of ordered contours and push it into the
 * renderer.
 *
 * @param render          Renderer that owns the primitive stack.
 * @param cam_view        Owning cam view (used for theme colors and the app).
 * @param name            Part name (typically the source file name).
 * @param smoothing       Ramer-Douglas-Peucker epsilon stored on the Part.
 * @param view_callback   Transform callback assigned to the Part.
 * @param mouse_callback  Mouse-event callback assigned to the Part.
 * @param all_chains      One ordered contour per entry.
 * @param chain_layers    Parallel to all_chains: destination layer per contour.
 * @param chain_tolerance Endpoints closer than this mark a contour closed.
 * @return The pushed Part (owned by the renderer), or nullptr if no geometry.
 */
Part* buildAndPushPart(
  NcRender*                       render,
  NcCamView*                      cam_view,
  const std::string&              name,
  float                           smoothing,
  std::function<void(Primitive*)> view_callback,
  std::function<void(Primitive*, const Primitive::MouseEventData&)>
                                           mouse_callback,
  const std::vector<std::vector<Point2d>>& all_chains,
  const std::vector<std::string>&          chain_layers,
  double                                   chain_tolerance);

} // namespace path_import

#endif
