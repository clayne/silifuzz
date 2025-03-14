# Copyright 2022 The SiliFuzz Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# SiliFuzz Orchestrator.
# A binary that orchestrates the execution of SiliFuzz runners.

load("@rules_python//python:py_test.bzl", "py_test")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "silifuzz_orchestrator_main",
    srcs = ["silifuzz_orchestrator_main.cc"],
    features = ["fully_static_link"],
    linkstatic = 1,
    deps = [
        ":corpus_util",
        ":orchestrator_util",
        ":result_collector",
        ":silifuzz_orchestrator",
        "@silifuzz//proto:corpus_metadata_cc_proto",
        "@silifuzz//runner/driver:runner_options",
        "@silifuzz//util:checks",
        "@silifuzz//util:cpu_id",
        "@silifuzz//util:span_util",
        "@silifuzz//util:tool_util",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log:flags",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "silifuzz_orchestrator",
    srcs = ["silifuzz_orchestrator.cc"],
    hdrs = ["silifuzz_orchestrator.h"],
    deps = [
        ":corpus_util",
        "@silifuzz//runner/driver:runner_driver",
        "@silifuzz//runner/driver:runner_options",
        "@silifuzz//util:checks",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "silifuzz_orchestrator_test",
    size = "medium",
    timeout = "short",
    srcs = ["silifuzz_orchestrator_test.cc"],
    deps = [
        ":silifuzz_orchestrator",
        "@silifuzz//runner/driver:runner_driver",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "result_collector",
    srcs = ["result_collector.cc"],
    hdrs = ["result_collector.h"],
    deps = [
        ":binary_log_channel",
        ":orchestrator_util",
        "@silifuzz//common:snapshot_enums",
        "@silifuzz//player:player_result_proto",
        "@silifuzz//proto:binary_log_entry_cc_proto",
        "@silifuzz//proto:corpus_metadata_cc_proto",
        "@silifuzz//proto:session_summary_cc_proto",
        "@silifuzz//proto:snapshot_execution_result_cc_proto",
        "@silifuzz//runner/driver:runner_driver",
        "@silifuzz//util:checks",
        "@silifuzz//util:cpu_id",
        "@silifuzz//util:hostname",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "result_collector_test",
    size = "medium",
    timeout = "short",
    srcs = ["result_collector_test.cc"],
    deps = [
        ":binary_log_channel",
        ":result_collector",
        "@silifuzz//proto:binary_log_entry_cc_proto",
        "@silifuzz//proto:snapshot_execution_result_cc_proto",
        "@silifuzz//runner/driver:runner_driver",
        "@silifuzz//util/testing:status_macros",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "orchestrator_util",
    srcs = ["orchestrator_util.cc"],
    hdrs = ["orchestrator_util.h"],
    deps = [
        ":corpus_util",
        "@silifuzz//util:checks",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "orchestrator_util_test",
    size = "medium",
    srcs = ["orchestrator_util_test.cc"],
    # dd if=/dev/zero of=/dev/stdout bs=1024 count=1024 | xz -9 > testdata/one_mb_of_zeros.xz
    data = ["testdata/one_mb_of_zeros.xz"],
    deps = [
        ":orchestrator_util",
        "@silifuzz//util:data_dependency",
        "@silifuzz//util:subprocess",
        "@silifuzz//util/testing:status_macros",
        "@silifuzz//util/testing:status_matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "corpus_util",
    srcs = ["corpus_util.cc"],
    hdrs = ["corpus_util.h"],
    deps = [
        "@silifuzz//snap",
        "@silifuzz//snap:snap_checksum",
        "@silifuzz//util:byte_io",
        "@silifuzz//util:checks",
        "@silifuzz//util:itoa",
        "@silifuzz//util:math",
        "@silifuzz//util:owned_file_descriptor",
        "@silifuzz//util:path_util",
        "@silifuzz//util:span_util",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:span",
        "@liblzma",
    ],
)

cc_test(
    name = "corpus_util_test",
    srcs = ["corpus_util_test.cc"],
    data = [
        # dd if=/dev/zero of=/dev/stdout bs=1024 count=1024 | xz -9 > testdata/one_mb_of_zeros.xz
        "testdata/one_mb_of_zeros.xz",
        # dd if=/dev/zero of=/dev/stdout bs=1024 count=2048 | xz -9 > testdata/two_mb_of_zeros.xz
        "testdata/two_mb_of_zeros.xz",
    ],
    deps = [
        ":corpus_util",
        "@silifuzz//snap",
        "@silifuzz//util:byte_io",
        "@silifuzz//util:data_dependency",
        "@silifuzz//util:owned_file_descriptor",
        "@silifuzz//util/testing:status_macros",
        "@silifuzz//util/testing:status_matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "test_runner",
    srcs = ["test_runner.cc"],
    deps = [
        "@silifuzz//snap",
        "@silifuzz//snap:snap_corpus_util",
        "@silifuzz//util:arch",
        "@silifuzz//util:mmapped_memory_ptr",
    ],
)

py_test(
    name = "orchestrator_test",
    size = "medium",
    srcs = ["orchestrator_test.py"],
    data = [
        ":silifuzz_orchestrator_main",
        ":test_runner",
        "@silifuzz//snap/testing:ends_as_expected_corpus",
        "@silifuzz//snap/testing:runaway_corpus",
    ],
    deps = [
        "@absl_py//absl/logging",
        "@absl_py//absl/testing:absltest",
        "@absl_py//absl/testing:parameterized",
        "@silifuzz//proto:binary_log_entry_py_pb2",
    ],
)

cc_library(
    name = "binary_log_channel",
    srcs = ["binary_log_channel.cc"],
    hdrs = ["binary_log_channel.h"],
    deps = [
        "@silifuzz//proto:binary_log_entry_cc_proto",
        "@silifuzz//proto:snapshot_execution_result_cc_proto",
        "@silifuzz//util:byte_io",
        "@silifuzz//util:checks",
        "@silifuzz//util:itoa",
        "@silifuzz//util:signals",
        "@com_google_absl//absl/base:endian",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "binary_log_channel_test",
    srcs = ["binary_log_channel_test.cc"],
    deps = [
        ":binary_log_channel",
        "@silifuzz//proto:binary_log_entry_cc_proto",
        "@silifuzz//proto:player_result_cc_proto",
        "@silifuzz//proto:snapshot_execution_result_cc_proto",
        "@silifuzz//util:byte_io",
        "@silifuzz//util:checks",
        "@silifuzz//util:time_proto_util",
        "@silifuzz//util/testing:status_macros",
        "@silifuzz//util/testing:status_matchers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
