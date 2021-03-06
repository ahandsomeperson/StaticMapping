// MIT License

// Copyright (c) 2019 Edward Liu

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// stl
#include <algorithm>
#include <cfloat>
#include <iostream>
// local
#include "common/math.h"

namespace static_map {
namespace common {

// refer to paper :
// "A Fast Voxel Traversal Algorithm for Ray Tracing"
// John Amanatides, 1987
// BUG( infinite loop ) not recommended to use
std::vector<Eigen::Vector3i> VoxelCasting(const Eigen::Vector3f& ray_start,
                                          const Eigen::Vector3f& ray_end,
                                          const float& step_size) {
  std::vector<Eigen::Vector3i> visited_voxels;
  // Compute ray direction.
  Eigen::Vector3f ray = ray_end - ray_start;

  // This id of the first/current voxel hit by the ray.
  // Using floor (round down) is actually very important,
  // the implicit int-casting will round up for negative numbers.
  Eigen::Vector3i start_voxel(std::floor(ray_start[0] / step_size),
                              std::floor(ray_start[1] / step_size),
                              std::floor(ray_start[2] / step_size));
  // The id of the last voxel hit by the ray.
  // @todo what happens if the end point is on a border?
  Eigen::Vector3i end_voxel(std::floor(ray_end[0] / step_size),
                            std::floor(ray_end[1] / step_size),
                            std::floor(ray_end[2] / step_size));
  Eigen::Vector3i current_voxel = start_voxel;

  if (start_voxel == end_voxel) {
    visited_voxels.push_back(start_voxel);
    return visited_voxels;
  }
  // In which direction the voxel ids are incremented.
  int step_x = (ray[0] >= 0.) ? 1 : -1;
  int step_y = (ray[1] >= 0.) ? 1 : -1;
  int step_z = (ray[2] >= 0.) ? 1 : -1;
  // Distance along the ray to the next voxel border from the current position
  // (tMaxX, tMaxY, tMaxZ).
  float next_voxel_boundary_x =
      (current_voxel[0] + step_x) * step_size;  // correct
  float next_voxel_boundary_y =
      (current_voxel[1] + step_y) * step_size;  // correct
  float next_voxel_boundary_z =
      (current_voxel[2] + step_z) * step_size;  // correct
  // tMaxX, tMaxY, tMaxZ -- distance until next intersection with voxel-border
  // the value of t at which the ray crosses the first vertical voxel boundary
  float tMaxX = (start_voxel[0] != end_voxel[0])
                    ? (next_voxel_boundary_x - ray_start[0]) / ray[0]
                    : FLT_MAX;
  float tMaxY = (start_voxel[1] != end_voxel[1])
                    ? (next_voxel_boundary_y - ray_start[1]) / ray[1]
                    : FLT_MAX;
  float tMaxZ = (start_voxel[2] != end_voxel[2])
                    ? (next_voxel_boundary_z - ray_start[2]) / ray[2]
                    : FLT_MAX;
  // tDeltaX, tDeltaY, tDeltaZ --
  // how far along the ray we must move for the horizontal component to equal
  // the width of a voxel the direction in which we traverse the grid can only
  // be FLT_MAX if we never go in that direction
  float tDeltaX =
      (start_voxel[0] != end_voxel[0]) ? step_size / ray[0] * step_x : FLT_MAX;
  float tDeltaY =
      (start_voxel[1] != end_voxel[1]) ? step_size / ray[1] * step_y : FLT_MAX;
  float tDeltaZ =
      (start_voxel[2] != end_voxel[2]) ? step_size / ray[2] * step_z : FLT_MAX;

  Eigen::Vector3i diff(0, 0, 0);
  bool neg_ray = false;
  for (int i = 0; i < 3; ++i) {
    if (current_voxel[i] != end_voxel[i] && ray[i] < 0.) {
      diff[i]--;
      neg_ray = true;
    }
  }
  visited_voxels.push_back(current_voxel);
  if (neg_ray) {
    current_voxel += diff;
    visited_voxels.push_back(current_voxel);
  }

  while (end_voxel != current_voxel) {
    if (tMaxX < tMaxY) {
      if (tMaxX < tMaxZ) {
        current_voxel[0] += step_x;
        tMaxX += tDeltaX;
      } else {
        current_voxel[2] += step_z;
        tMaxZ += tDeltaZ;
      }
    } else {
      if (tMaxY < tMaxZ) {
        current_voxel[1] += step_y;
        tMaxY += tDeltaY;
      } else {
        current_voxel[2] += step_z;
        tMaxZ += tDeltaZ;
      }
    }
    visited_voxels.push_back(current_voxel);
  }
  return visited_voxels;
}

// refer to code in
// "http://members.chello.at/easyfilter/bresenham.html"
std::vector<Eigen::Vector3i> VoxelCastingBresenham(
    const Eigen::Vector3f& ray_start, const Eigen::Vector3f& ray_end,
    const float& step_size) {
  std::vector<Eigen::Vector3i> visited_voxels;
  // This id of the first/current voxel hit by the ray.
  // Using floor (round down) is actually very important,
  // the implicit int-casting will round up for negative numbers.
  int x0 = std::lround(std::floor(ray_start[0] / step_size));
  int y0 = std::lround(std::floor(ray_start[1] / step_size));
  int z0 = std::lround(std::floor(ray_start[2] / step_size));
  if (std::isnan(x0) || std::isnan(y0) || std::isnan(z0)) {
    return visited_voxels;
  }
  // The id of the last voxel hit by the ray.
  // @todo what happens if the end point is on a border?
  int x1 = std::lround(std::floor(ray_end[0] / step_size));
  int y1 = std::lround(std::floor(ray_end[1] / step_size));
  int z1 = std::lround(std::floor(ray_end[2] / step_size));
  if (std::isnan(x1) || std::isnan(y1) || std::isnan(z1)) {
    return visited_voxels;
  }

  const int x1_init = x1;
  const int y1_init = y1;
  const int z1_init = z1;

  const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  const int dy = std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  const int dz = std::abs(z1 - z0), sz = z0 < z1 ? 1 : -1;
  const int dm = std::max({dx, dy, dz});
  x1 = y1 = z1 = (dm >> 1); /* error offset */

  int i = dm;
  visited_voxels.reserve(dm + 1);
  for (;;) {
    visited_voxels.emplace_back(x0, y0, z0);
    if (i-- == 0) {
      break;
    }
    x1 -= dx;
    if (x1 < 0) {
      x1 += dm;
      x0 += sx;
    }
    y1 -= dy;
    if (y1 < 0) {
      y1 += dm;
      y0 += sy;
    }
    z1 -= dz;
    if (z1 < 0) {
      z1 += dm;
      z0 += sz;
    }
  }

  CHECK_EQ(x0, x1_init);
  CHECK_EQ(y0, y1_init);
  CHECK_EQ(z0, z1_init);
  return visited_voxels;
}

// 1.7~2 times fast then the function VoxelCasting
// proved useful
std::vector<Eigen::Vector3i> VoxelCastingDDA(const Eigen::Vector3f& ray_start,
                                             const Eigen::Vector3f& ray_end,
                                             const float& step_size) {
  std::vector<Eigen::Vector3i> visited_voxels;
  // This id of the first/current voxel hit by the ray.
  // Using floor (round down) is actually very important,
  // the implicit int-casting will round up for negative numbers.
  Eigen::Vector3i start_voxel(
      static_cast<int>(std::lround(std::floor(ray_start[0] / step_size))),
      static_cast<int>(std::lround(std::floor(ray_start[1] / step_size))),
      static_cast<int>(std::lround(std::floor(ray_start[2] / step_size))));
  if (std::isnan(start_voxel[0]) || std::isnan(start_voxel[1]) ||
      std::isnan(start_voxel[2])) {
    return visited_voxels;
  }
  // The id of the last voxel hit by the ray.
  // @todo what happens if the end point is on a border?
  Eigen::Vector3i end_voxel(
      static_cast<int>(std::lround(std::floor(ray_end[0] / step_size))),
      static_cast<int>(std::lround(std::floor(ray_end[1] / step_size))),
      static_cast<int>(std::lround(std::floor(ray_end[2] / step_size))));
  if (std::isnan(end_voxel[0]) || std::isnan(end_voxel[1]) ||
      std::isnan(end_voxel[2])) {
    return visited_voxels;
  }

  Eigen::Vector3i current_voxel = start_voxel;
  Eigen::Vector3i delta = end_voxel - start_voxel;
  Eigen::Vector3i error(0, 0, 0);
  Eigen::Vector3i step(delta[0] >= 0 ? 1 : -1, delta[1] >= 0 ? 1 : -1,
                       delta[2] >= 0 ? 1 : -1);

  // take the absolute value of the coordinate changes
  delta[0] = delta[0] < 0 ? -delta[0] : delta[0];
  delta[1] = delta[1] < 0 ? -delta[1] : delta[1];
  delta[2] = delta[2] < 0 ? -delta[2] : delta[2];

  int max_delta = delta[0] > delta[1] ? delta[0] : delta[1];
  max_delta = max_delta > delta[2] ? max_delta : delta[2];

  visited_voxels.reserve(max_delta + 1);
  for (int i = 0; i < max_delta; ++i) {
    visited_voxels.push_back(current_voxel);

    // Bresenham error updates
    error[0] += delta[0];
    error[1] += delta[1];
    error[2] += delta[2];

    // check if error in x exceeds threshold, if so
    // update coord and error
    if ((error[0] << 1) >= max_delta) {
      current_voxel[0] += step[0];
      error[0] -= max_delta;
    }
    // check if error in y exceeds threshold, if so
    // update coord and error
    if ((error[1] << 1) >= max_delta) {
      current_voxel[1] += step[1];
      error[1] -= max_delta;
    }

    if ((error[2] << 1) >= max_delta) {
      current_voxel[2] += step[2];
      error[2] -= max_delta;
    }
  }
  visited_voxels.push_back(current_voxel);
  // visited_voxels.shrink_to_fit();
  if (current_voxel != end_voxel) {
    std::cout << current_voxel.transpose() << " != " << end_voxel.transpose()
              << std::endl;
  }
  CHECK_EQ(current_voxel, end_voxel);
  return visited_voxels;
}

}  // namespace common
}  // namespace static_map
