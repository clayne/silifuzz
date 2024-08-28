// Copyright 2022 The SiliFuzz Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "./util/cpu_id.h"

#include <sched.h>

#include "gtest/gtest.h"
#include "./util/checks.h"
#include "./util/itoa.h"

namespace silifuzz {
namespace {

double RecordConsistency(int (*callback)(), int expected) {
  constexpr int kNumIterations = 100;
  int is_expected_count = 0;
  for (int i = 0; i < kNumIterations; i++) {
    sched_yield();
    if (callback() == expected) {
      is_expected_count += 1;
    }
  }
  return static_cast<double>(is_expected_count) / kNumIterations;
}

TEST(CPUId, BasicTest) {
  double normal_consistency_sum = 0;
  double nosys_consistency_sum = 0;
  int num_trials = 0;

  ForEachAvailableCPU([&](int cpu) {
    if (SetCPUAffinity(cpu) != 0) {
      LOG_ERROR("Cannot bind to CPU ", IntStr(cpu));
      return;
    }
    normal_consistency_sum += RecordConsistency(&GetCPUId, cpu);
    nosys_consistency_sum += RecordConsistency(&GetCPUIdNoSyscall, cpu);
    num_trials += 1;
  });

  EXPECT_GE(num_trials, 0);

  // This is chosen empirically to keep failure rate below 1 in 10000.
  constexpr double kAcceptableErrorRate = 0.10;
  EXPECT_GE(normal_consistency_sum, num_trials * (1.0 - kAcceptableErrorRate));
  EXPECT_GE(nosys_consistency_sum, num_trials * (1.0 - kAcceptableErrorRate));
}

}  // namespace
}  // namespace silifuzz
