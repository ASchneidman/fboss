// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <folly/String.h>
#include <folly/dynamic.h>
#include <gtest/gtest.h>

#include <fboss/thrift_cow/nodes/Serializer.h>
#include <fboss/thrift_cow/storage/CowStorage.h>
#include <thrift/lib/cpp2/reflection/folly_dynamic.h>
#include "fboss/facebook/fsdb/oper/ExtendedPathBuilder.h"
#include "fboss/fsdb/tests/gen-cpp2-thriftpath/thriftpath_test.h" // @manual=//fboss/fsdb/tests:thriftpath_test_thrift-cpp2-thriftpath
#include "fboss/fsdb/tests/gen-cpp2/thriftpath_test_fatal_types.h"
#include "fboss/fsdb/tests/gen-cpp2/thriftpath_test_types.h"

using folly::dynamic;

namespace {

using namespace facebook::fboss::fsdb;
dynamic createTestDynamic() {
  return dynamic::object("tx", true)(
      "rx",
      false)("name", "testname")("optionalString", "bla")("enumeration", 1)("enumMap", dynamic::object)("member", dynamic::object("min", 10)("max", 20))("variantMember", dynamic::object("integral", 99))("structMap", dynamic::object(3, dynamic::object("min", 100)("max", 200)))("structList", dynamic::array())("enumSet", dynamic::array())("integralSet", dynamic::array())("mapOfStringToI32", dynamic::object())("listOfPrimitives", dynamic::array())("setOfI32", dynamic::array())("stringToStruct", dynamic::object())("listTypedef", dynamic::array());
}

TestStruct createTestStructForExtendedTests() {
  auto testDyn = createTestDynamic();
  for (int i = 0; i <= 20; ++i) {
    testDyn["mapOfStringToI32"][fmt::format("test{}", i)] = i;
    testDyn["listOfPrimitives"].push_back(i);
    testDyn["setOfI32"].push_back(i);
  }

  return apache::thrift::from_dynamic<TestStruct>(
      testDyn, apache::thrift::dynamic_format::JSON_1);
}

} // namespace

#ifdef ENABLE_DYNAMIC_APIS
TEST(CowStorageTests, GetDynamic) {
  using namespace facebook::fboss::fsdb;

  thriftpath::RootThriftPath<TestStruct> root;

  auto testDyn = createTestDynamic();
  auto testStruct = apache::thrift::from_dynamic<TestStruct>(
      testDyn, apache::thrift::dynamic_format::JSON_1);
  auto storage = CowStorage<TestStruct>(testStruct);

  EXPECT_EQ(storage.get_dynamic(root.tx()).value(), true);
  EXPECT_EQ(storage.get_dynamic(root.rx()).value(), false);
  EXPECT_EQ(storage.get_dynamic(root.member()).value(), testDyn["member"]);
  EXPECT_EQ(
      storage.get_dynamic(root.structMap()[3]).value(),
      testDyn["structMap"][3]);
  EXPECT_EQ(storage.get_dynamic(root).value(), testDyn);
}

TEST(CowStorageTests, SetDynamic) {
  using namespace facebook::fboss::fsdb;

  thriftpath::RootThriftPath<TestStruct> root;

  auto testDyn = createTestDynamic();
  auto testStruct = apache::thrift::from_dynamic<TestStruct>(
      testDyn, apache::thrift::dynamic_format::JSON_1);
  auto storage = CowStorage<TestStruct>(testStruct);

  XLOG(INFO) << "getting root.tx";
  EXPECT_EQ(storage.get_dynamic(root.tx()).value(), true);
  XLOG(INFO) << "getting root.rx";
  EXPECT_EQ(storage.get_dynamic(root.rx()).value(), false);
  EXPECT_EQ(storage.get_dynamic(root.member()).value(), testDyn["member"]);
  EXPECT_EQ(
      storage.get_dynamic(root.structMap()[3]).value(),
      testDyn["structMap"][3]);

  // change all the fields
  XLOG(INFO) << "setting root.tx=false";
  EXPECT_EQ(storage.set_dynamic(root.tx(), false), std::nullopt);
  XLOG(INFO) << "getting root.tx";
  EXPECT_EQ(storage.get_dynamic(root.tx()).value(), false);
  XLOG(INFO) << "setting root.tx=false again";
  EXPECT_EQ(storage.set_dynamic(root.tx(), false), std::nullopt);
  XLOG(INFO) << "getting root.tx again";
  EXPECT_EQ(storage.get_dynamic(root.tx()).value(), false);

  XLOG(INFO) << "setting root.rx=true";
  EXPECT_EQ(storage.set_dynamic(root.rx(), true), std::nullopt);
  dynamic newMember = dynamic::object("min", 500)("max", 5000);
  dynamic newStructMapMember = dynamic::object("min", 300)("max", 3000);
  EXPECT_EQ(storage.set_dynamic(root.member(), newMember), std::nullopt);
  EXPECT_EQ(
      storage.set_dynamic(root.structMap()[3], newStructMapMember),
      std::nullopt);

  XLOG(INFO) << "getting root.rx";
  EXPECT_EQ(storage.get_dynamic(root.rx()).value(), true);
  EXPECT_EQ(storage.get_dynamic(root.member()).value(), newMember);
  EXPECT_EQ(
      storage.get_dynamic(root.structMap()[3]).value(), newStructMapMember);
}
#endif

TEST(CowStorageTests, GetThrift) {
  using namespace facebook::fboss::fsdb;

  thriftpath::RootThriftPath<TestStruct> root;

  auto testStruct = apache::thrift::from_dynamic<TestStruct>(
      createTestDynamic(), apache::thrift::dynamic_format::JSON_1);
  auto storage = CowStorage<TestStruct>(testStruct);

  EXPECT_EQ(storage.get(root.tx()).value(), true);
  EXPECT_EQ(storage.get(root.rx()).value(), false);
  EXPECT_EQ(storage.get(root.member()).value(), testStruct.member().value());
  EXPECT_EQ(
      storage.get(root.structMap()[3]).value(), testStruct.structMap()->at(3));
  EXPECT_EQ(storage.get(root).value(), testStruct);
}

TEST(CowStorageTests, GetEncoded) {
  using namespace facebook::fboss::fsdb;

  thriftpath::RootThriftPath<TestStruct> root;

  auto testStruct = apache::thrift::from_dynamic<TestStruct>(
      createTestDynamic(), apache::thrift::dynamic_format::JSON_1);
  auto storage = CowStorage<TestStruct>(testStruct);

  auto result = storage.get_encoded(root.tx(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(
      *result->contents(),
      facebook::fboss::thrift_cow::serialize<
          apache::thrift::type_class::integral>(
          OperProtocol::SIMPLE_JSON, true));
  result = storage.get_encoded(root.rx(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(
      *result->contents(),
      facebook::fboss::thrift_cow::serialize<
          apache::thrift::type_class::integral>(
          OperProtocol::SIMPLE_JSON, false));
  result = storage.get_encoded(root.member(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(
      *result->contents(),
      facebook::fboss::thrift_cow::serialize<
          apache::thrift::type_class::structure>(
          OperProtocol::SIMPLE_JSON, *testStruct.member()));
  result = storage.get_encoded(root.structMap()[3], OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(
      *result->contents(),
      facebook::fboss::thrift_cow::serialize<
          apache::thrift::type_class::structure>(
          OperProtocol::SIMPLE_JSON, testStruct.structMap()->at(3)));
  result = storage.get_encoded(root, OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(
      *result->contents(),
      facebook::fboss::thrift_cow::serialize<
          apache::thrift::type_class::structure>(
          OperProtocol::SIMPLE_JSON, testStruct));
}

TEST(CowStorageTests, GetEncodedMetadata) {
  using namespace facebook::fboss::fsdb;

  thriftpath::RootThriftPath<TestStruct> root;

  auto testStruct = apache::thrift::from_dynamic<TestStruct>(
      createTestDynamic(), apache::thrift::dynamic_format::JSON_1);
  auto storage = CowStorage<TestStruct>(testStruct);

  auto result = storage.get_encoded(root.tx(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(
      *result->contents(),
      facebook::fboss::thrift_cow::serialize<
          apache::thrift::type_class::integral>(
          OperProtocol::SIMPLE_JSON, true));
  result = storage.get_encoded(root, OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(
      *result->contents(),
      facebook::fboss::thrift_cow::serialize<
          apache::thrift::type_class::structure>(
          OperProtocol::SIMPLE_JSON, testStruct));

  storage.publish();
  EXPECT_TRUE(storage.isPublished());

#ifdef ENABLE_DYNAMIC_APIS
  // change tx to false, since we published already, this should clone
  EXPECT_EQ(storage.set_dynamic(root.tx(), false), std::nullopt);
#endif

  result = storage.get_encoded(root.tx(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(
      *result->contents(),
      facebook::fboss::thrift_cow::serialize<
          apache::thrift::type_class::integral>(
          OperProtocol::SIMPLE_JSON, false));

  result = storage.get_encoded(root, OperProtocol::SIMPLE_JSON);
  auto testStruct2 = testStruct;
  testStruct2.tx() = false;
  EXPECT_EQ(
      *result->contents(),
      facebook::fboss::thrift_cow::serialize<
          apache::thrift::type_class::structure>(
          OperProtocol::SIMPLE_JSON, testStruct2));
}

TEST(CowStorageTests, SetThrift) {
  using namespace facebook::fboss::fsdb;

  thriftpath::RootThriftPath<TestStruct> root;

  auto testStruct = apache::thrift::from_dynamic<TestStruct>(
      createTestDynamic(), apache::thrift::dynamic_format::JSON_1);
  auto storage = CowStorage<TestStruct>(testStruct);

  EXPECT_EQ(storage.get(root.tx()).value(), true);
  EXPECT_EQ(storage.get(root.rx()).value(), false);
  EXPECT_EQ(storage.get(root.member()).value(), testStruct.member().value());
  EXPECT_EQ(
      storage.get(root.structMap()[3]).value(), testStruct.structMap()->at(3));

  TestStructSimple newMember;
  newMember.min() = 500;
  newMember.max() = 5000;
  TestStructSimple newStructMapMember;
  newStructMapMember.min() = 300;
  newStructMapMember.max() = 3000;

  // change all the fields
  EXPECT_EQ(storage.set(root.tx(), false), std::nullopt);
  EXPECT_EQ(storage.set(root.rx(), true), std::nullopt);
  EXPECT_EQ(storage.set(root.member(), newMember), std::nullopt);
  EXPECT_EQ(storage.set(root.structMap()[3], newStructMapMember), std::nullopt);

  EXPECT_EQ(storage.get(root.tx()).value(), false);
  EXPECT_EQ(storage.get(root.rx()).value(), true);
  EXPECT_EQ(storage.get(root.member()).value(), newMember);
  EXPECT_EQ(storage.get(root.structMap()[3]).value(), newStructMapMember);
}

TEST(CowStorageTests, AddThrift) {
  using namespace facebook::fboss::fsdb;

  thriftpath::RootThriftPath<TestStruct> root;

  auto testStruct = apache::thrift::from_dynamic<TestStruct>(
      createTestDynamic(), apache::thrift::dynamic_format::JSON_1);
  auto storage = CowStorage<TestStruct>(testStruct);

  EXPECT_EQ(storage.get(root.tx()).value(), true);
  EXPECT_EQ(storage.get(root.rx()).value(), false);
  EXPECT_EQ(storage.get(root.member()).value(), testStruct.member().value());
  EXPECT_EQ(
      storage.get(root.structMap()[3]).value(), testStruct.structMap()->at(3));

  TestStructSimple member1;
  member1.min() = 500;
  member1.max() = 5000;
  TestStructSimple member2;
  member2.min() = 300;
  member2.max() = 3000;

  // add values
  EXPECT_EQ(storage.add(root.structMap()[1], member1), std::nullopt);
  EXPECT_EQ(storage.add(root.structMap()[2], member2), std::nullopt);
  // EXPECT_EQ(storage.add(root.structList()[-1], member1), std::nullopt);
  EXPECT_EQ(storage.add(root.structList()[0], member2), std::nullopt);
  EXPECT_EQ(
      storage.add(root.enumMap()[TestEnum::FIRST], member2), std::nullopt);

  EXPECT_EQ(storage.get(root.structMap()[1]).value(), member1);
  EXPECT_EQ(storage.get(root.structMap()[2]).value(), member2);
  EXPECT_EQ(storage.get(root.structList()[0]).value(), member2);
  EXPECT_EQ(storage.get(root.enumMap()[TestEnum::FIRST]).value(), member2);

  std::vector<std::string> testPath = {"enumMap", "FIRST"};
  EXPECT_EQ(storage.template get<TestStructSimple>(testPath).value(), member2);
}

TEST(CowStorageTests, AddDynamic) {
  using namespace facebook::fboss::fsdb;

  thriftpath::RootThriftPath<TestStruct> root;

  auto testStruct = apache::thrift::from_dynamic<TestStruct>(
      createTestDynamic(), apache::thrift::dynamic_format::JSON_1);
  auto storage = CowStorage<TestStruct>(testStruct);

  EXPECT_EQ(storage.get(root.tx()).value(), true);
  EXPECT_EQ(storage.get(root.rx()).value(), false);
  EXPECT_EQ(storage.get(root.member()).value(), testStruct.member().value());
  EXPECT_EQ(
      storage.get(root.structMap()[3]).value(), testStruct.structMap()->at(3));

  folly::dynamic member1 = dynamic::object("min", 500)("max", 5000);
  folly::dynamic member2 = dynamic::object("min", 300)("max", 3000);

#ifdef ENABLE_DYNAMIC_APIS
  // add values
  EXPECT_EQ(storage.add_dynamic(root.structMap()[1], member1), std::nullopt);
  EXPECT_EQ(storage.add_dynamic(root.structMap()[2], member2), std::nullopt);
  // EXPECT_EQ(
  //   storage.add_dynamic(root.structList()[-1], member1), std::nullopt);
  EXPECT_EQ(storage.add_dynamic(root.structList()[0], member2), std::nullopt);

  EXPECT_EQ(storage.get_dynamic(root.structMap()[1]).value(), member1);
  EXPECT_EQ(storage.get_dynamic(root.structMap()[2]).value(), member2);
  EXPECT_EQ(storage.get_dynamic(root.structList()[0]).value(), member2);
// EXPECT_EQ(storage.get_dynamic(root.structList()[1]).value(), member1);
#endif
}

TEST(CowStorageTests, RemoveThrift) {
  using namespace facebook::fboss::fsdb;

  thriftpath::RootThriftPath<TestStruct> root;

  auto testStruct = apache::thrift::from_dynamic<TestStruct>(
      createTestDynamic(), apache::thrift::dynamic_format::JSON_1);
  auto storage = CowStorage<TestStruct>(testStruct);

  EXPECT_EQ(storage.get(root.tx()).value(), true);
  EXPECT_EQ(storage.get(root.rx()).value(), false);
  EXPECT_EQ(storage.get(root.member()).value(), testStruct.member().value());
  EXPECT_EQ(
      storage.get(root.structMap()[3]).value(), testStruct.structMap()->at(3));

  TestStructSimple member1;
  member1.min() = 500;
  member1.max() = 5000;
  TestStructSimple member2;
  member2.min() = 300;
  member2.max() = 3000;

  // add values
  EXPECT_EQ(storage.add(root.structMap()[1], member1), std::nullopt);
  EXPECT_EQ(storage.add(root.structMap()[2], member2), std::nullopt);
  EXPECT_EQ(storage.add(root.structList()[0], member2), std::nullopt);
  EXPECT_EQ(storage.add(root.structList()[1], member1), std::nullopt);
  EXPECT_EQ(storage.add(root.structList()[2], member1), std::nullopt);

  EXPECT_EQ(storage.get(root.structMap()[1]).value(), member1);
  EXPECT_EQ(storage.get(root.structMap()[2]).value(), member2);
  EXPECT_EQ(storage.get(root.structList()[0]).value(), member2);
  EXPECT_EQ(storage.get(root.structList()[1]).value(), member1);
  EXPECT_EQ(storage.get(root.structList()[2]).value(), member1);

  // delete values
  EXPECT_EQ(storage.remove(root.structMap()[2]), std::nullopt);
  EXPECT_EQ(storage.remove(root.structMap()[3]), std::nullopt);
  EXPECT_EQ(storage.remove(root.structList()[0]), std::nullopt);
  EXPECT_EQ(storage.remove(root.structList()[10]), std::nullopt);

  EXPECT_EQ(storage.get(root.structMap()[1]).value(), member1);
  EXPECT_EQ(
      storage.get(root.structMap()[2]).error(), StorageError::INVALID_PATH);
  EXPECT_EQ(
      storage.get(root.structMap()[3]).error(), StorageError::INVALID_PATH);
  EXPECT_EQ(storage.get(root.structList()[0]).value(), member1);
  EXPECT_EQ(storage.get(root.structList()[1]).value(), member1);
  EXPECT_EQ(
      storage.get(root.structList()[2]).error(), StorageError::INVALID_PATH);
  EXPECT_EQ(
      storage.get(root.structList()[3]).error(), StorageError::INVALID_PATH);
  EXPECT_EQ(
      storage.get(root.structList()[5]).error(), StorageError::INVALID_PATH);
}

TEST(CowStorageTests, PatchDelta) {
  using namespace facebook::fboss::fsdb;
  using namespace apache::thrift::type_class;

  thriftpath::RootThriftPath<TestStruct> root;

  auto testStruct = apache::thrift::from_dynamic<TestStruct>(
      createTestDynamic(), apache::thrift::dynamic_format::JSON_1);
  auto storage = CowStorage<TestStruct>(testStruct);

  // publish to ensure we can patch published storage
  storage.publish();
  EXPECT_TRUE(storage.isPublished());

  EXPECT_EQ(storage.get(root.tx()).value(), true);
  EXPECT_EQ(storage.get(root.rx()).value(), false);
  EXPECT_EQ(storage.get(root.optionalString()).value(), "bla");

  auto makeState = [](auto tc, auto val) -> folly::fbstring {
    OperState state;
    using TC = decltype(tc);
    return facebook::fboss::thrift_cow::serialize<TC>(
        OperProtocol::SIMPLE_JSON, val);
  };

  auto deltaUnit = [](std::vector<std::string> path,
                      std::optional<folly::fbstring> oldState,
                      std::optional<folly::fbstring> newState) {
    OperDeltaUnit unit;
    unit.path()->raw() = std::move(path);
    if (oldState) {
      unit.oldState() = *oldState;
    }
    if (newState) {
      unit.newState() = *newState;
    }
    return unit;
  };

  // add values
  OperDelta delta;

  std::vector<OperDeltaUnit> changes = {
      deltaUnit({"tx"}, std::nullopt, makeState(integral{}, false)),
      deltaUnit({"rx"}, std::nullopt, makeState(integral{}, true)),
      deltaUnit({"optionalString"}, makeState(string{}, "bla"), std::nullopt),
      deltaUnit({"member", "min"}, std::nullopt, makeState(integral{}, 100)),
      deltaUnit(
          {"structMap", "5", "min"}, std::nullopt, makeState(integral{}, 1001)),
      deltaUnit(
          {"enumMap", "FIRST", "min"},
          std::nullopt,
          makeState(integral{}, 2001))};
  delta.changes() = std::move(changes);
  delta.protocol() = OperProtocol::SIMPLE_JSON;
  storage.patch(delta);

  EXPECT_EQ(storage.get(root.tx()).value(), false);
  EXPECT_EQ(storage.get(root.rx()).value(), true);
  EXPECT_EQ(
      storage.get(root.optionalString()).error(), StorageError::INVALID_PATH);
  EXPECT_EQ(storage.get(root.member().min()).value(), 100);
  EXPECT_EQ(storage.get(root.structMap()[5].min()).value(), 1001);
  EXPECT_EQ(storage.get(root.enumMap()[TestEnum::FIRST].min()).value(), 2001);
}

TEST(CowStorageTests, EncodedExtendedAccessFieldSimple) {
  auto testStruct = createTestStructForExtendedTests();

  auto storage = CowStorage<TestStruct>(testStruct);
  storage.publish();
  EXPECT_TRUE(storage.isPublished());

  auto path = ext_path_builder::raw("tx").get();
  auto result = storage.get_encoded_extended(
      path.path()->begin(), path.path()->end(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(result->size(), 1);
  EXPECT_EQ(
      *result->at(0).state()->contents(),
      facebook::fboss::thrift_cow::serialize<
          apache::thrift::type_class::integral>(
          OperProtocol::SIMPLE_JSON, true));
}

TEST(CowStorageTests, EncodedExtendedAccessFieldInContainer) {
  auto testStruct = createTestStructForExtendedTests();

  auto storage = CowStorage<TestStruct>(testStruct);
  storage.publish();
  EXPECT_TRUE(storage.isPublished());

  auto path = ext_path_builder::raw("structMap").raw("3").get();
  auto result = storage.get_encoded_extended(
      path.path()->begin(), path.path()->end(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(result->size(), 1);
  auto got = facebook::fboss::thrift_cow::
      deserialize<apache::thrift::type_class::structure, TestStructSimple>(
          OperProtocol::SIMPLE_JSON, *result->at(0).state()->contents());
  EXPECT_EQ(*got.min(), 100);
  EXPECT_EQ(*got.max(), 200);
}

TEST(CowStorageTests, EncodedExtendedAccessRegexMap) {
  auto testStruct = createTestStructForExtendedTests();

  auto storage = CowStorage<TestStruct>(testStruct);
  storage.publish();
  EXPECT_TRUE(storage.isPublished());

  std::map<std::vector<std::string>, int> expected = {
      {{"mapOfStringToI32", "test1"}, 1},
      {{"mapOfStringToI32", "test10"}, 10},
      {{"mapOfStringToI32", "test11"}, 11},
      {{"mapOfStringToI32", "test12"}, 12},
      {{"mapOfStringToI32", "test13"}, 13},
      {{"mapOfStringToI32", "test14"}, 14},
      {{"mapOfStringToI32", "test15"}, 15},
      {{"mapOfStringToI32", "test16"}, 16},
      {{"mapOfStringToI32", "test17"}, 17},
      {{"mapOfStringToI32", "test18"}, 18},
      {{"mapOfStringToI32", "test19"}, 19},
  };

  auto path = ext_path_builder::raw("mapOfStringToI32").regex("test1.*").get();
  auto result = storage.get_encoded_extended(
      path.path()->begin(), path.path()->end(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(expected.size(), result->size());
  for (const auto& taggedState : *result) {
    const auto& elemPath = *taggedState.path()->path();
    const auto& contents = *taggedState.state()->contents();
    auto deserialized = facebook::fboss::thrift_cow::
        deserialize<apache::thrift::type_class::integral, int>(
            OperProtocol::SIMPLE_JSON, contents);
    EXPECT_EQ(expected[elemPath], deserialized)
        << "Mismatch at /" + folly::join('/', elemPath);
  }
}

TEST(CowStorageTests, EncodedExtendedAccessAnyMap) {
  auto testStruct = createTestStructForExtendedTests();

  auto storage = CowStorage<TestStruct>(testStruct);
  storage.publish();
  EXPECT_TRUE(storage.isPublished());

  std::map<std::vector<std::string>, int> expected = {
      {{"mapOfStringToI32", "test0"}, 0},
      {{"mapOfStringToI32", "test1"}, 1},
      {{"mapOfStringToI32", "test2"}, 2},
      {{"mapOfStringToI32", "test3"}, 3},
      {{"mapOfStringToI32", "test4"}, 4},
      {{"mapOfStringToI32", "test5"}, 5},
      {{"mapOfStringToI32", "test6"}, 6},
      {{"mapOfStringToI32", "test7"}, 7},
      {{"mapOfStringToI32", "test8"}, 8},
      {{"mapOfStringToI32", "test9"}, 9},
      {{"mapOfStringToI32", "test10"}, 10},
      {{"mapOfStringToI32", "test11"}, 11},
      {{"mapOfStringToI32", "test12"}, 12},
      {{"mapOfStringToI32", "test13"}, 13},
      {{"mapOfStringToI32", "test14"}, 14},
      {{"mapOfStringToI32", "test15"}, 15},
      {{"mapOfStringToI32", "test16"}, 16},
      {{"mapOfStringToI32", "test17"}, 17},
      {{"mapOfStringToI32", "test18"}, 18},
      {{"mapOfStringToI32", "test19"}, 19},
      {{"mapOfStringToI32", "test20"}, 20},
  };
  auto path = ext_path_builder::raw("mapOfStringToI32").any().get();
  auto result = storage.get_encoded_extended(
      path.path()->begin(), path.path()->end(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(expected.size(), result->size());
  for (const auto& taggedState : *result) {
    const auto& elemPath = *taggedState.path()->path();
    const auto& contents = *taggedState.state()->contents();
    auto deserialized = facebook::fboss::thrift_cow::
        deserialize<apache::thrift::type_class::integral, int>(
            OperProtocol::SIMPLE_JSON, contents);
    EXPECT_EQ(expected[elemPath], deserialized)
        << "Mismatch at /" + folly::join('/', elemPath);
  }
}

TEST(CowStorageTests, EncodedExtendedAccessRegexList) {
  auto testStruct = createTestStructForExtendedTests();

  auto storage = CowStorage<TestStruct>(testStruct);
  storage.publish();
  EXPECT_TRUE(storage.isPublished());

  std::map<std::vector<std::string>, int> expected = {
      {{"listOfPrimitives", "1"}, 1},
      {{"listOfPrimitives", "10"}, 10},
      {{"listOfPrimitives", "11"}, 11},
      {{"listOfPrimitives", "12"}, 12},
      {{"listOfPrimitives", "13"}, 13},
      {{"listOfPrimitives", "14"}, 14},
      {{"listOfPrimitives", "15"}, 15},
      {{"listOfPrimitives", "16"}, 16},
      {{"listOfPrimitives", "17"}, 17},
      {{"listOfPrimitives", "18"}, 18},
      {{"listOfPrimitives", "19"}, 19},
  };
  auto path = ext_path_builder::raw("listOfPrimitives").regex("1.*").get();
  auto result = storage.get_encoded_extended(
      path.path()->begin(), path.path()->end(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(expected.size(), result->size());
  for (const auto& taggedState : *result) {
    const auto& elemPath = *taggedState.path()->path();
    const auto& contents = *taggedState.state()->contents();
    auto deserialized = facebook::fboss::thrift_cow::
        deserialize<apache::thrift::type_class::integral, int>(
            OperProtocol::SIMPLE_JSON, contents);
    EXPECT_EQ(expected[elemPath], deserialized)
        << "Mismatch at /" + folly::join('/', elemPath);
  }
}

TEST(CowStorageTests, EncodedExtendedAccessAnyList) {
  auto testStruct = createTestStructForExtendedTests();

  auto storage = CowStorage<TestStruct>(testStruct);
  storage.publish();
  EXPECT_TRUE(storage.isPublished());

  std::map<std::vector<std::string>, int> expected = {
      {{"listOfPrimitives", "0"}, 0},   {{"listOfPrimitives", "1"}, 1},
      {{"listOfPrimitives", "2"}, 2},   {{"listOfPrimitives", "3"}, 3},
      {{"listOfPrimitives", "4"}, 4},   {{"listOfPrimitives", "5"}, 5},
      {{"listOfPrimitives", "6"}, 6},   {{"listOfPrimitives", "7"}, 7},
      {{"listOfPrimitives", "8"}, 8},   {{"listOfPrimitives", "9"}, 9},
      {{"listOfPrimitives", "10"}, 10}, {{"listOfPrimitives", "11"}, 11},
      {{"listOfPrimitives", "12"}, 12}, {{"listOfPrimitives", "13"}, 13},
      {{"listOfPrimitives", "14"}, 14}, {{"listOfPrimitives", "15"}, 15},
      {{"listOfPrimitives", "16"}, 16}, {{"listOfPrimitives", "17"}, 17},
      {{"listOfPrimitives", "18"}, 18}, {{"listOfPrimitives", "19"}, 19},
      {{"listOfPrimitives", "20"}, 20},
  };

  auto path = ext_path_builder::raw("listOfPrimitives").any().get();
  auto result = storage.get_encoded_extended(
      path.path()->begin(), path.path()->end(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(expected.size(), result->size());
  for (const auto& taggedState : *result) {
    const auto& elemPath = *taggedState.path()->path();
    const auto& contents = *taggedState.state()->contents();
    auto deserialized = facebook::fboss::thrift_cow::
        deserialize<apache::thrift::type_class::integral, int>(
            OperProtocol::SIMPLE_JSON, contents);
    EXPECT_EQ(expected[elemPath], deserialized)
        << "Mismatch at /" + folly::join('/', elemPath);
  }
}

TEST(CowStorageTests, EncodedExtendedAccessRegexSet) {
  auto testStruct = createTestStructForExtendedTests();

  auto storage = CowStorage<TestStruct>(testStruct);
  storage.publish();
  EXPECT_TRUE(storage.isPublished());

  std::map<std::vector<std::string>, int> expected = {
      {{"setOfI32", "1"}, 1},
      {{"setOfI32", "10"}, 10},
      {{"setOfI32", "11"}, 11},
      {{"setOfI32", "12"}, 12},
      {{"setOfI32", "13"}, 13},
      {{"setOfI32", "14"}, 14},
      {{"setOfI32", "15"}, 15},
      {{"setOfI32", "16"}, 16},
      {{"setOfI32", "17"}, 17},
      {{"setOfI32", "18"}, 18},
      {{"setOfI32", "19"}, 19},
  };

  auto path = ext_path_builder::raw("setOfI32").regex("1.*").get();
  auto result = storage.get_encoded_extended(
      path.path()->begin(), path.path()->end(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(expected.size(), result->size());
  for (const auto& taggedState : *result) {
    const auto& elemPath = *taggedState.path()->path();
    const auto& contents = *taggedState.state()->contents();
    auto deserialized = facebook::fboss::thrift_cow::
        deserialize<apache::thrift::type_class::integral, int>(
            OperProtocol::SIMPLE_JSON, contents);
    EXPECT_EQ(expected[elemPath], deserialized)
        << "Mismatch at /" + folly::join('/', elemPath);
  }
}

TEST(CowStorageTests, EncodedExtendedAccessAnySet) {
  auto testStruct = createTestStructForExtendedTests();

  auto storage = CowStorage<TestStruct>(testStruct);
  storage.publish();
  EXPECT_TRUE(storage.isPublished());

  std::map<std::vector<std::string>, int> expected = {
      {{"setOfI32", "0"}, 0},   {{"setOfI32", "1"}, 1},
      {{"setOfI32", "2"}, 2},   {{"setOfI32", "3"}, 3},
      {{"setOfI32", "4"}, 4},   {{"setOfI32", "5"}, 5},
      {{"setOfI32", "6"}, 6},   {{"setOfI32", "7"}, 7},
      {{"setOfI32", "8"}, 8},   {{"setOfI32", "9"}, 9},
      {{"setOfI32", "10"}, 10}, {{"setOfI32", "11"}, 11},
      {{"setOfI32", "12"}, 12}, {{"setOfI32", "13"}, 13},
      {{"setOfI32", "14"}, 14}, {{"setOfI32", "15"}, 15},
      {{"setOfI32", "16"}, 16}, {{"setOfI32", "17"}, 17},
      {{"setOfI32", "18"}, 18}, {{"setOfI32", "19"}, 19},
      {{"setOfI32", "20"}, 20},
  };

  auto path = ext_path_builder::raw("setOfI32").any().get();
  auto result = storage.get_encoded_extended(
      path.path()->begin(), path.path()->end(), OperProtocol::SIMPLE_JSON);
  EXPECT_EQ(expected.size(), result->size());
  for (const auto& taggedState : *result) {
    const auto& elemPath = *taggedState.path()->path();
    const auto& contents = *taggedState.state()->contents();
    auto deserialized = facebook::fboss::thrift_cow::
        deserialize<apache::thrift::type_class::integral, int>(
            OperProtocol::SIMPLE_JSON, contents);
    EXPECT_EQ(expected[elemPath], deserialized)
        << "Mismatch at /" + folly::join('/', elemPath);
  }
}