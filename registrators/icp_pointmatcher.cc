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

#include "registrators/icp_pointmatcher.h"

namespace static_map {
namespace registrator {

template <typename PointType>
bool IcpUsingPointMatcher<PointType>::align(const Eigen::Matrix4f& guess,
                                            Eigen::Matrix4f& result) {
  // **** compute the transform ****
  result = pm_icp_.compute(*reading_cloud_, *reference_cloud_, guess);

  // **** compute the final score ****
  PM::DataPoints data_out(*reading_cloud_);
  pm_icp_.transformations.apply(data_out, result);
  pm_icp_.matcher->init(*reference_cloud_);

  // extract closest points
  PM::Matches matches = pm_icp_.matcher->findClosests(data_out);

  // weight paired points
  const PM::OutlierWeights outlier_weights =
      pm_icp_.outlierFilters.compute(data_out, *reference_cloud_, matches);

  // generate tuples of matched points and remove pairs with zero weight
  const PM::ErrorMinimizer::ErrorElements matched_points(
      data_out, *reference_cloud_, outlier_weights, matches);

  // extract relevant information for convenience
  const int dim = matched_points.reading.getEuclideanDim();
  const int matcher_points_num = matched_points.reading.getNbPoints();
  const PM::Matrix matched_read = matched_points.reading.features.topRows(dim);
  const PM::Matrix matched_ref = matched_points.reference.features.topRows(dim);

  CHECK(matched_read.rows() == matched_ref.rows() &&
        matched_read.cols() == matched_ref.cols() && matched_read.rows() == 3);

  Interface<PointType>::point_pairs_.ref_points =
      matched_ref.transpose().cast<float>();
  Interface<PointType>::point_pairs_.read_points =
      matched_read.transpose().cast<float>();
  Interface<PointType>::point_pairs_.pairs_num = matched_read.cols();

  // compute mean distance
  const PM::Matrix dist = (matched_read - matched_ref).colwise().norm();
  // replace that by squaredNorm() to save computation time
  const float mean_dist = dist.sum() / static_cast<float>(matcher_points_num);
  // cout << "Robust mean distance: " << mean_dist << " m" << endl;
  Interface<PointType>::final_score_ = std::exp(-mean_dist);

  if (Interface<PointType>::final_score_ < 0.6) {
    return false;
  }
  return true;
}

template <typename PointType>
void IcpUsingPointMatcher<PointType>::loadConfig(
    const std::string& yaml_filename) {
  if (yaml_filename.empty()) {
    loadDefaultConfig();
    return;
  }

  // load YAML config
  std::ifstream ifs(yaml_filename.c_str());
  if (!ifs.good()) {
    PRINT_ERROR_FMT("Cannot open config file: %s", yaml_filename.c_str());
    exit(1);
  }
  pm_icp_.loadFromYaml(ifs);
}

template <typename PointType>
void IcpUsingPointMatcher<PointType>::loadDefaultConfig() {
  // config
  PointMatcherSupport::Parametrizable::Parameters params;
  std::string name;

  name = "RandomSamplingDataPointsFilter";
  params["prob"] = "0.9";
  std::shared_ptr<PM::DataPointsFilter> rand_read =
      PM::get().DataPointsFilterRegistrar.create(name, params);
  params.clear();

  name = "SamplingSurfaceNormalDataPointsFilter";
  params["knn"] = "7";
  params["samplingMethod"] = "1";
  params["ratio"] = "0.1";
  std::shared_ptr<PM::DataPointsFilter> normal_ref =
      PM::get().DataPointsFilterRegistrar.create(name, params);
  params.clear();

  // Prepare matching function
  name = "KDTreeMatcher";
  params["knn"] = "1";
  params["epsilon"] = "3.16";
  // params["maxDist"] = "0.5";
  std::shared_ptr<PM::Matcher> kdtree =
      PM::get().MatcherRegistrar.create(name, params);
  params.clear();

  // Prepare outlier filters
  name = "TrimmedDistOutlierFilter";
  params["ratio"] = "0.7";
  std::shared_ptr<PM::OutlierFilter> trim =
      PM::get().OutlierFilterRegistrar.create(name, params);
  params.clear();

  // Prepare error minimization
  name = "PointToPointErrorMinimizer";
  std::shared_ptr<PM::ErrorMinimizer> pointToPoint =
      PM::get().ErrorMinimizerRegistrar.create(name);

  name = "PointToPlaneErrorMinimizer";
  std::shared_ptr<PM::ErrorMinimizer> pointToPlane =
      PM::get().ErrorMinimizerRegistrar.create(name);

  // Prepare transformation checker filters
  name = "CounterTransformationChecker";
  params["maxIterationCount"] = "150";
  std::shared_ptr<PM::TransformationChecker> maxIter =
      PM::get().TransformationCheckerRegistrar.create(name, params);
  params.clear();

  name = "DifferentialTransformationChecker";
  params["minDiffRotErr"] = "0.001";
  params["minDiffTransErr"] = "0.01";
  params["smoothLength"] = "4";
  std::shared_ptr<PM::TransformationChecker> diff =
      PM::get().TransformationCheckerRegistrar.create(name, params);
  params.clear();

  std::shared_ptr<PM::Transformation> rigidTrans =
      PM::get().TransformationRegistrar.create("RigidTransformation");
  // Prepare inspector
  std::shared_ptr<PM::Inspector> nullInspect =
      PM::get().InspectorRegistrar.create("NullInspector");

  // data filters
  pm_icp_.readingDataPointsFilters.push_back(rand_read);
  pm_icp_.referenceDataPointsFilters.push_back(normal_ref);
  // matcher
  pm_icp_.matcher = kdtree;
  // outlier filter
  pm_icp_.outlierFilters.push_back(trim);
  // error minimizer
  pm_icp_.errorMinimizer = pointToPlane;
  // checker
  pm_icp_.transformationCheckers.push_back(maxIter);
  pm_icp_.transformationCheckers.push_back(diff);

  pm_icp_.inspector = nullInspect;
  // result transform
  pm_icp_.transformations.push_back(rigidTrans);
}

template class IcpUsingPointMatcher<pcl::PointXYZI>;
template class IcpUsingPointMatcher<pcl::PointXYZ>;

}  // namespace registrator
}  // namespace static_map
