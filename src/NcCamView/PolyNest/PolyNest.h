#ifndef POLY_NEST_
#define POLY_NEST_

#include <NcRender/geometry/clipper.h>
#include <loguru.hpp>
#include <NanoCut.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace PolyNest {

class PolyPoint {
public:
  double x;
  double y;
  PolyPoint() {};
  PolyPoint(double x, double y)
  {
    this->x = x;
    this->y = y;
  };
};

class PolyGon {
public:
  ClipperLib::Path m_vertices;
  bool             m_is_closed;
  bool             m_is_inside;
};

class PolyPart {
public:
  std::vector<PolyGon> m_polygons;
  std::vector<PolyGon> m_built_polygons;
  PolyPoint   m_bbox_min;
  PolyPoint   m_bbox_max;
  double*     m_offset_x;
  double*     m_offset_y;
  double*     m_angle;
  bool*       m_visible;
  std::string m_part_name;
  PolyPart()
  {
    m_bbox_min.x = 0;
    m_bbox_min.y = 0;
    m_bbox_max.x = 0;
    m_bbox_max.y = 0;
    m_part_name = "";
  };
  PolyPoint rotatePoint(PolyPoint p, double angle);
  void      getBoundingBox(PolyPoint* bbox_min, PolyPoint* bbox_max);
  void      moveOutsidePolyGonToBack();
  void      build();
};

// NFP cache key: identifies the NFP between two contours at specific angles
struct NfpKey {
  size_t stat_idx;
  size_t orb_idx;
  double stat_angle;
  double orb_angle;
  bool operator==(const NfpKey& other) const
  {
    return stat_idx == other.stat_idx && orb_idx == other.orb_idx &&
           stat_angle == other.stat_angle && orb_angle == other.orb_angle;
  }
};

struct NfpKeyHash {
  size_t operator()(const NfpKey& k) const
  {
    size_t h = std::hash<size_t>{}(k.stat_idx);
    h ^= std::hash<size_t>{}(k.orb_idx) << 1;
    h ^= std::hash<double>{}(k.stat_angle) << 2;
    h ^= std::hash<double>{}(k.orb_angle) << 3;
    return h;
  }
};

// Result of a complete nesting attempt
struct NestingSolution {
  std::vector<size_t> part_order;
  std::vector<double> part_angles;
  std::vector<double> placed_x;
  std::vector<double> placed_y;
  std::vector<bool>   placed_ok;
  int    parts_placed = 0;
  double bounding_width = 0;
};

// Record of an already-placed part (fixed during nesting)
struct FixedPart {
  size_t              part_idx;
  double              angle;
  double              x;
  double              y;
  ClipperLib::Path    contour; // outside contour at base angle=0
};

class PolyNest {
private:
  PolyPoint             m_min_extents;
  PolyPoint             m_max_extents;
  double                m_closed_tolerance;
  std::vector<PolyPart> m_unplaced_parts;
  std::vector<PolyPart> m_placed_parts;

  // NFP algorithm state
  std::unordered_map<NfpKey, ClipperLib::Paths, NfpKeyHash> m_nfp_cache;
  ClipperLib::Path       m_material_rect;
  std::vector<FixedPart> m_fixed_placed;
  std::vector<double>    m_allowed_rotations;

  // SA parameters
  double m_sa_initial_temp    = 1.0;
  double m_sa_cooling_rate    = 0.9995;
  int    m_sa_max_iterations  = 10000;

  // Part building helpers (kept from original)
  bool   checkIfPointIsInsidePath(std::vector<PolyPoint> path, PolyPoint point);
  bool   checkIfPathIsInsidePath(std::vector<PolyPoint> path1,
                                 std::vector<PolyPoint> path2);
  double measureDistanceBetweenPoints(PolyPoint a, PolyPoint b);
  PolyPart buildPart(std::vector<std::vector<PolyPoint>> p,
                     double* offset_x, double* offset_y,
                     double* angle, bool* visible);

  // Base contour storage: [0..F-1] = fixed parts, [F..F+N-1] = unplaced parts
  std::vector<ClipperLib::Path> m_base_contours;
  size_t m_num_fixed_contours = 0;

  // NFP utilities
  static ClipperLib::Path reflectPolygon(const ClipperLib::Path& poly);
  static ClipperLib::Path rotatePolygon(const ClipperLib::Path& poly, double angle);
  ClipperLib::Path getBaseContour(const PolyPart& part) const;
  ClipperLib::Paths computeNfp(const ClipperLib::Path& stationary,
                               const ClipperLib::Path& orbiting);
  ClipperLib::Paths computeIfp(const ClipperLib::Path& part_contour);
  const ClipperLib::Paths& getNfpCached(size_t stat_idx, double stat_angle,
                                        size_t orb_idx, double orb_angle);

  // BLF placement
  NestingSolution runBLF(const std::vector<size_t>& order,
                         const std::vector<double>& angles);
  ClipperLib::FPoint findBottomLeftPoint(const ClipperLib::Paths& feasible);

  // Simulated annealing
  NestingSolution runSimulatedAnnealing(std::atomic<float>* progress);
  double evaluateFitness(const NestingSolution& sol);

public:
  PolyNest()
  {
    m_min_extents = PolyPoint(0, 0);
    m_max_extents = PolyPoint(SCALE(1000), SCALE(1000));
    m_closed_tolerance = SCALE(0.1);
    m_allowed_rotations = { 0, 90, 180, 270 };
  };
  void setExtents(PolyPoint min, PolyPoint max);
  void pushUnplacedPolyPart(std::vector<std::vector<PolyPoint>> p,
                            double* offset_x, double* offset_y,
                            double* angle, bool* visible);
  void pushPlacedPolyPart(std::vector<std::vector<PolyPoint>> p,
                          double* offset_x, double* offset_y,
                          double* angle, bool* visible);
  void beginPlaceUnplacedPolyParts();
  int  placeAllUnplacedParts(std::atomic<float>* progress); // Returns number of parts that failed to place
};

}; // namespace PolyNest

#endif
