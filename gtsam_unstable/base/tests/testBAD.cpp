/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file testBAD.cpp
 * @date September 18, 2014
 * @author Frank Dellaert
 * @brief unit tests for Block Automatic Differentiation
 */

#include <gtsam/nonlinear/NonlinearFactor.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/geometry/Cal3_S2.h>
#include <gtsam/slam/GeneralSFMFactor.h>
#include <gtsam/inference/Key.h>
#include <gtsam/base/Testable.h>

#include <boost/make_shared.hpp>

#include <CppUnitLite/TestHarness.h>

namespace gtsam {

/// Base class
template<class T>
class Expression {

public:

  typedef Expression<T> This;
  typedef boost::shared_ptr<This> shared_ptr;

  virtual T value(const Values& values) const = 0;
};

/// Constant Expression
template<class T>
class ConstantExpression: public Expression<T> {

  T value_;

public:

  /// Constructor with a value, yielding a constant
  ConstantExpression(const T& value) :
      value_(value) {
  }

  T value(const Values& values) const {
    return value_;
  }
};

/// Leaf Expression
template<class T>
class LeafExpression: public Expression<T> {

  Key key_;

public:

  /// Constructor with a single key
  LeafExpression(Key key) {
  }

  T value(const Values& values) const {
    return values.at<T>(key_);
  }
};

/// Expression version of transform
LeafExpression<Point3> transformTo(const Expression<Pose3>& x,
    const Expression<Point3>& p) {
  return LeafExpression<Point3>(0);
}

/// Expression version of project
LeafExpression<Point2> project(const Expression<Point3>& p) {
  return LeafExpression<Point2>(0);
}

/// Expression version of uncalibrate
LeafExpression<Point2> uncalibrate(const Expression<Cal3_S2>& K,
    const Expression<Point2>& p) {
  return LeafExpression<Point2>(0);
}

/// Expression version of Point2.sub
LeafExpression<Point2> operator -(const Expression<Point2>& p,
    const Expression<Point2>& q) {
  return LeafExpression<Point2>(0);
}

/// AD Factor
template<class T, class E>
class BADFactor: NonlinearFactor {

  const T measurement_;
  const E expression_;

public:

  /// Constructor
  BADFactor(const T& measurement, const E& expression) :
      measurement_(measurement), expression_(expression) {
  }

  /**
   * Calculate the error of the factor
   * This is typically equal to log-likelihood, e.g. \f$ 0.5(h(x)-z)^2/sigma^2 \f$
   */
  double error(const Values& c) const {
    return 0;
  }

  /// get the dimension of the factor (number of rows on linearization)
  size_t dim() const {
    return 0;
  }

  /// linearize to a GaussianFactor
  boost::shared_ptr<GaussianFactor> linearize(const Values& values) const {
    // We will construct an n-ary factor below, where  terms is a container whose
    // value type is std::pair<Key, Matrix>, specifying the
    // collection of keys and matrices making up the factor.
    std::map<Key, Matrix> terms;
    const T& value = expression_.value(values);
    Vector b = measurement_.localCoordinates(value);
    SharedDiagonal model = SharedDiagonal();
    return boost::shared_ptr<JacobianFactor>(
        new JacobianFactor(terms, b, model));
  }

};
}

using namespace std;
using namespace gtsam;

/* ************************************************************************* */

TEST(BAD, test) {

  // Create some values
  Values values;
  values.insert(1, Pose3());
  values.insert(2, Point3(0, 0, 1));
  values.insert(3, Cal3_S2());

  // Create old-style factor to create expected value and derivatives
  Point2 measured(0, 1);
  SharedNoiseModel model = noiseModel::Unit::Create(2);
  GeneralSFMFactor2<Cal3_S2> old(measured, model, 1, 2, 3);
  GaussianFactor::shared_ptr expected = old.linearize(values);

  // Create leaves
  LeafExpression<Pose3> x(1);
  LeafExpression<Point3> p(2);
  LeafExpression<Cal3_S2> K(3);

  // Create expression tree
  LeafExpression<Point3> p_cam = transformTo(x, p);
  LeafExpression<Point2> projection = project(p_cam);
  LeafExpression<Point2> uv_hat = uncalibrate(K, projection);

  // Create factor
  BADFactor<Point2, LeafExpression<Point2> > f(measured, uv_hat);

  // Check value
  EXPECT_DOUBLES_EQUAL(old.error(values), f.error(values), 1e-9);

  // Check dimension
  EXPECT_LONGS_EQUAL(0, f.dim());

  // Check linearization
  boost::shared_ptr<GaussianFactor> gf = f.linearize(values);
  EXPECT( assert_equal(*expected, *gf, 1e-9));
}

/* ************************************************************************* */
int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}
/* ************************************************************************* */
