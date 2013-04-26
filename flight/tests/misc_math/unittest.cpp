#include "gtest/gtest.h"

#include <stdio.h>		/* printf */
#include <stdlib.h>		/* abort */
#include <string.h>		/* memset */
#include <stdint.h>		/* uint*_t */

extern "C" {

#include "misc_math.h"		/* API for misc_math functions */

}

// To use a test fixture, derive a class from testing::Test.
class MiscMath : public testing::Test {
protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
};

// Test fixture for bound_min_max()
class BoundMinMax : public MiscMath {
protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
};

TEST_F(BoundMinMax, ValBelowZeroRange) {
  // Test lower bounding when min = max with (val < min)
  EXPECT_EQ(-1.0f, bound_min_max(-10.0f, -1.0f, -1.0f));
  EXPECT_EQ(0.0f, bound_min_max(-10.0f, 0.0f, 0.0f));
  EXPECT_EQ(1.0f, bound_min_max(-10.0f, 1.0f, 1.0f));
};

TEST_F(BoundMinMax, ValWithinZeroRange) {
  // Test bounding when min = max = val
  EXPECT_EQ(-1.0f, bound_min_max(-1.0f, -1.0f, -1.0f));
  EXPECT_EQ(0.0f, bound_min_max(0.0f, 0.0f, 0.0f));
  EXPECT_EQ(1.0f, bound_min_max(1.0f, 1.0f, 1.0f));
};

TEST_F(BoundMinMax, ValAboveZeroRange) {
  // Test upper bounding when min = max with (val > max)
  EXPECT_EQ(-1.0f, bound_min_max(10.0f, -1.0f, -1.0f));
  EXPECT_EQ(0.0f, bound_min_max(10.0f, 0.0f, 0.0f));
  EXPECT_EQ(1.0f, bound_min_max(10.0f, 1.0f, 1.0f));
}

TEST_F(BoundMinMax, PositiveMinMax) {
  float min = 1.0f;
  float max = 10.0f;

  // Below Lower Bound
  EXPECT_EQ(min, bound_min_max(min - 1.0f, min, max));
  // At Lower Bound
  EXPECT_EQ(min, bound_min_max(min, min, max));
  // In Bounds
  EXPECT_EQ(2.0f, bound_min_max(2.0f, min, max));
  // At Upper Bound
  EXPECT_EQ(max, bound_min_max(max, min, max));
  // Above Upper Bound
  EXPECT_EQ(max, bound_min_max(max + 1.0f, min, max));
}

TEST_F(BoundMinMax, NegativeMinMax) {
  float min = -10.0f;
  float max = -1.0f;

  // Below Lower Bound
  EXPECT_EQ(min, bound_min_max(min - 1.0f, min, max));
  // At Lower Bound
  EXPECT_EQ(min, bound_min_max(min, min, max));
  // In Bounds
  EXPECT_EQ(-2.0f, bound_min_max(-2.0f, min, max));
  // At Upper Bound
  EXPECT_EQ(max, bound_min_max(max, min, max));
  // Above Upper Bound
  EXPECT_EQ(max, bound_min_max(max + 1.0f, min, max));
}

TEST_F(BoundMinMax, StraddleZeroMinMax) {
  float min = -10.0f;
  float max = 10.0f;

  // Below Lower Bound
  EXPECT_EQ(min, bound_min_max(min - 1.0f, min, max));
  // At Lower Bound
  EXPECT_EQ(min, bound_min_max(min, min, max));
  // In Bounds
  EXPECT_EQ(0.0f, bound_min_max(0.0f, min, max));
  // At Upper Bound
  EXPECT_EQ(max, bound_min_max(max, min, max));
  // Above Upper Bound
  EXPECT_EQ(max, bound_min_max(max + 1.0f, min, max));
}

TEST_F(BoundMinMax, DISABLED_ReversedMinMax) {
  float min = 10.0f;
  float max = -10.0f;

  // Below Lower Bound
  EXPECT_EQ(min, bound_min_max(min - 1.0f, min, max));
  // At Lower Bound
  EXPECT_EQ(min, bound_min_max(min, min, max));
  // In Bounds
  EXPECT_EQ(0.0f, bound_min_max(0.0f, min, max));
  // At Upper Bound
  EXPECT_EQ(max, bound_min_max(max, min, max));
  // Above Upper Bound
  EXPECT_EQ(max, bound_min_max(max + 1.0f, min, max));
}

// Test fixture for bound_sym()
class BoundSym : public MiscMath {
protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
};

TEST_F(BoundSym, ZeroRange) {
  float range = 0.0f;

  // Below Lower Bound
  EXPECT_EQ(-range, bound_sym(-range - 1.0f, range));
  // At Lower Bound
  EXPECT_EQ(-range, bound_sym(-range, range));
  // In Bounds
  EXPECT_EQ(0.0f, bound_sym(0.0f, range));
  // At Upper Bound
  EXPECT_EQ(range, bound_sym(range, range));
  // Above Upper Bound
  EXPECT_EQ(range, bound_sym(range + 1.0f, range));
};

TEST_F(BoundSym, NonZeroRange) {
  float range = 10.0f;

  // Below Lower Bound
  EXPECT_EQ(-range, bound_sym(-range - 1.0f, range));
  // At Lower Bound
  EXPECT_EQ(-range, bound_sym(-range, range));
  // In Bounds
  EXPECT_EQ(0.0f, bound_sym(0.0f, range));
  // At Upper Bound
  EXPECT_EQ(range, bound_sym(range, range));
  // Above Upper Bound
  EXPECT_EQ(range, bound_sym(range + 1.0f, range));
};

// Test fixture for circular_modulus_deg()
class CircularModulusDeg : public MiscMath {
protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
};

TEST_F(CircularModulusDeg, NullError) {
  float error = 0.0f;
  EXPECT_EQ(-error, circular_modulus_deg(error - 1080));
  EXPECT_EQ(-error, circular_modulus_deg(error - 720));
  EXPECT_EQ(-error, circular_modulus_deg(error - 360));
  EXPECT_EQ(-error, circular_modulus_deg(error));
  EXPECT_EQ(-error, circular_modulus_deg(error + 360));
  EXPECT_EQ(-error, circular_modulus_deg(error + 720));
  EXPECT_EQ(-error, circular_modulus_deg(error + 1080));
};

TEST_F(CircularModulusDeg, MaxPosError) {
  float error = 180.0f;
  EXPECT_EQ(-error, circular_modulus_deg(error - 1080));
  EXPECT_EQ(-error, circular_modulus_deg(error - 720));
  EXPECT_EQ(-error, circular_modulus_deg(error - 360));
  EXPECT_EQ(-error, circular_modulus_deg(error));
  EXPECT_EQ(-error, circular_modulus_deg(error + 360));
  EXPECT_EQ(-error, circular_modulus_deg(error + 720));
  EXPECT_EQ(-error, circular_modulus_deg(error + 1080));
};

TEST_F(CircularModulusDeg, MaxNegError) {
  float error = -180.0f;
  EXPECT_EQ(error, circular_modulus_deg(error - 1080));
  EXPECT_EQ(error, circular_modulus_deg(error - 720));
  EXPECT_EQ(error, circular_modulus_deg(error - 360));
  EXPECT_EQ(error, circular_modulus_deg(error));
  EXPECT_EQ(error, circular_modulus_deg(error + 360));
  EXPECT_EQ(error, circular_modulus_deg(error + 720));
  EXPECT_EQ(error, circular_modulus_deg(error + 1080));
};

TEST_F(CircularModulusDeg, SweepError) {
  float eps = 0.0001f;

  for (float error = -179.9f; error < 179.9f; error += 0.001f) {
    ASSERT_NEAR(error, circular_modulus_deg(error - 1080), eps);
    ASSERT_NEAR(error, circular_modulus_deg(error - 720), eps);
    ASSERT_NEAR(error, circular_modulus_deg(error - 360), eps);
    ASSERT_NEAR(error, circular_modulus_deg(error), eps);
    ASSERT_NEAR(error, circular_modulus_deg(error + 360), eps);
    ASSERT_NEAR(error, circular_modulus_deg(error + 720), eps);
    ASSERT_NEAR(error, circular_modulus_deg(error + 1080), eps);
  }
};
