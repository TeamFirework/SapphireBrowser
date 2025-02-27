// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"

#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolation_value.h"

namespace blink {

namespace {

class TestNonInterpolableValue : public NonInterpolableValue {
 public:
  ~TestNonInterpolableValue() final = default;

  static scoped_refptr<TestNonInterpolableValue> Create(int value) {
    DCHECK_GE(value, 1);
    return base::AdoptRef(new TestNonInterpolableValue(value));
  }

  int GetValue() const { return value_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  explicit TestNonInterpolableValue(int value) : value_(value) {}

  int value_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(TestNonInterpolableValue);

// DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS won't work in anonymous namespaces.
inline const TestNonInterpolableValue& ToTestNonInterpolableValue(
    const NonInterpolableValue& value) {
  DCHECK_EQ(value.GetType(), TestNonInterpolableValue::static_type_);
  return static_cast<const TestNonInterpolableValue&>(value);
}

// Creates an InterpolationValue containing a list of interpolable and
// non-interpolable values from the pairs of input.
InterpolationValue CreateInterpolableList(
    const std::vector<std::pair<double, int>>& values) {
  return ListInterpolationFunctions::CreateList(
      values.size(), [&values](size_t i) {
        return InterpolationValue(
            InterpolableNumber::Create(values[i].first),
            TestNonInterpolableValue::Create(values[i].second));
      });
}

// Creates an InterpolationValue which contains a list of interpolable values,
// but a non-interpolable list of nullptrs.
InterpolationValue CreateInterpolableList(const std::vector<double>& values) {
  return ListInterpolationFunctions::CreateList(values.size(), [&values](
                                                                   size_t i) {
    return InterpolationValue(InterpolableNumber::Create(values[i]), nullptr);
  });
}

bool NonInterpolableValuesAreCompatible(const NonInterpolableValue* a,
                                        const NonInterpolableValue* b) {
  // Note that '0' may never be held by TestNonInterpolableValues. See
  // DCHECK in TestNonInterpolableValue::Create.
  return (a ? ToTestNonInterpolableValue(*a).GetValue() : 0) ==
         (b ? ToTestNonInterpolableValue(*b).GetValue() : 0);
}

PairwiseInterpolationValue MaybeMergeSingles(InterpolationValue&& start,
                                             InterpolationValue&& end) {
  if (!NonInterpolableValuesAreCompatible(start.non_interpolable_value.get(),
                                          end.non_interpolable_value.get())) {
    return nullptr;
  }
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value), nullptr);
}

void Composite(
    std::unique_ptr<InterpolableValue>& underlying_interpolable_value,
    scoped_refptr<NonInterpolableValue>& underlying_non_interpolable_value,
    double underlying_fraction,
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value) {
  DCHECK(NonInterpolableValuesAreCompatible(
      underlying_non_interpolable_value.get(), non_interpolable_value));
  underlying_interpolable_value->ScaleAndAdd(underlying_fraction,
                                             interpolable_value);
}

}  // namespace

TEST(ListInterpolationFunctionsTest, EqualMergeSinglesSameLengths) {
  auto list1 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});
  auto list2 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});

  auto pairwise = ListInterpolationFunctions::MaybeMergeSingles(
      std::move(list1), std::move(list2),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      WTF::BindRepeating(MaybeMergeSingles));

  EXPECT_TRUE(pairwise);
}

TEST(ListInterpolationFunctionsTest, EqualMergeSinglesDifferentLengths) {
  auto list1 = CreateInterpolableList({1.0, 2.0, 3.0});
  auto list2 = CreateInterpolableList({1.0, 3.0});

  auto pairwise = ListInterpolationFunctions::MaybeMergeSingles(
      std::move(list1), std::move(list2),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      WTF::BindRepeating(MaybeMergeSingles));

  EXPECT_FALSE(pairwise);
}

TEST(ListInterpolationFunctionsTest, EqualMergeSinglesIncompatibleValues) {
  auto list1 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});
  auto list2 = CreateInterpolableList({{1.0, 1}, {2.0, 4}, {3.0, 3}});

  auto pairwise = ListInterpolationFunctions::MaybeMergeSingles(
      std::move(list1), std::move(list2),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      WTF::BindRepeating(MaybeMergeSingles));

  EXPECT_FALSE(pairwise);
}

TEST(ListInterpolationFunctionsTest, EqualMergeSinglesIncompatibleNullptrs) {
  auto list1 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});
  auto list2 = CreateInterpolableList({1, 2, 3});

  auto pairwise = ListInterpolationFunctions::MaybeMergeSingles(
      std::move(list1), std::move(list2),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      WTF::BindRepeating(MaybeMergeSingles));

  EXPECT_FALSE(pairwise);
}

TEST(ListInterpolationFunctionsTest, EqualCompositeSameLengths) {
  auto list1 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});
  auto list2 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});

  PropertyHandle property_handle(GetCSSPropertyZIndex());
  CSSNumberInterpolationType interpolation_type(property_handle);
  UnderlyingValueOwner owner;
  owner.Set(interpolation_type, std::move(list1));

  ListInterpolationFunctions::Composite(
      owner, 1.0, interpolation_type, list2,
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      WTF::BindRepeating(NonInterpolableValuesAreCompatible),
      WTF::BindRepeating(Composite));

  const auto& result = ToInterpolableList(*owner.Value().interpolable_value);

  ASSERT_EQ(result.length(), 3u);
  EXPECT_EQ(ToInterpolableNumber(result.Get(0))->Value(), 2.0);
  EXPECT_EQ(ToInterpolableNumber(result.Get(1))->Value(), 4.0);
  EXPECT_EQ(ToInterpolableNumber(result.Get(2))->Value(), 6.0);
}

// Two lists of different lengths are not interpolable, so we expect the
// underlying value to be replaced.
TEST(ListInterpolationFunctionsTest, EqualCompositeDifferentLengths) {
  auto list1 = CreateInterpolableList({1.0, 2.0, 3.0});
  auto list2 = CreateInterpolableList({4.0, 5.0});

  PropertyHandle property_handle(GetCSSPropertyZIndex());
  CSSNumberInterpolationType interpolation_type(property_handle);
  UnderlyingValueOwner owner;
  owner.Set(interpolation_type, std::move(list1));

  ListInterpolationFunctions::Composite(
      owner, 1.0, interpolation_type, list2,
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      WTF::BindRepeating(NonInterpolableValuesAreCompatible),
      WTF::BindRepeating(Composite));

  const auto& result = ToInterpolableList(*owner.Value().interpolable_value);

  ASSERT_EQ(result.length(), 2u);
  EXPECT_EQ(ToInterpolableNumber(result.Get(0))->Value(), 4.0);
  EXPECT_EQ(ToInterpolableNumber(result.Get(1))->Value(), 5.0);
}

// If one (or more) of the element pairs are incompatible, the list as a whole
// is non-interpolable. We expect the underlying value to be replaced.
TEST(ListInterpolationFunctionsTest, EqualCompositeIncompatibleValues) {
  auto list1 = CreateInterpolableList({{1.0, 1}, {2.0, 2}, {3.0, 3}});
  auto list2 = CreateInterpolableList({{4.0, 1}, {5.0, 4}, {6.0, 3}});

  PropertyHandle property_handle(GetCSSPropertyZIndex());
  CSSNumberInterpolationType interpolation_type(property_handle);
  UnderlyingValueOwner owner;
  owner.Set(interpolation_type, std::move(list1));

  ListInterpolationFunctions::Composite(
      owner, 1.0, interpolation_type, list2,
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      WTF::BindRepeating(NonInterpolableValuesAreCompatible),
      WTF::BindRepeating(Composite));

  const auto& result = ToInterpolableList(*owner.Value().interpolable_value);

  ASSERT_EQ(result.length(), 3u);
  EXPECT_EQ(ToInterpolableNumber(result.Get(0))->Value(), 4.0);
  EXPECT_EQ(ToInterpolableNumber(result.Get(1))->Value(), 5.0);
  EXPECT_EQ(ToInterpolableNumber(result.Get(2))->Value(), 6.0);
}

}  // namespace blink
