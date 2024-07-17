// Copyright 2023 The SiliFuzz Authors.
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

#ifndef THIRD_PARTY_SILIFUZZ_UTIL_CPU_FEATURES_H_
#define THIRD_PARTY_SILIFUZZ_UTIL_CPU_FEATURES_H_

#include "./util/itoa.h"

namespace silifuzz {

// A small subset of X86 CPU features that we want to know about.
// It is not our intention to cover all X86 CPU features.
enum class X86CPUFeatures {
  kBegin = 0,
  kAMX_TILE = kBegin,  // for accessing tile and tileconfig registers.
  kAVX,                // for accessing ymm registers.
  kAVX512BW,           // for accessing upper 48 bits of opmask registers.
  kAVX512F,  // for accessing zmm and lower 16 bits of opmask registers.
  kOSXSAVE,  // OS provides processor extended state management.
  kSSE,      // for accessing SSE registers.
  kSSE4_2,   // for CRC32 instructions.
  kXSAVE,    // CPU support XSAVE and related instructions.
  kEnd,      // One past the last valid value.
};

// The number of features is limited to be one fewer than the number of bits
// in a uint64_t for an implementation reason.
static_assert(static_cast<int>(X86CPUFeatures::kEnd) < 64);

// EnumStr() works for X86CPUFeatures.
template <>
inline constexpr const char*
    EnumNameMap<X86CPUFeatures>[static_cast<int>(X86CPUFeatures::kEnd)] = {
        "AMX_TILE", "AVX", "AVX512BW", "AVX512F",
        "OSXSAVE",  "SSE", "SSE4_2",   "XSAVE",
};

#ifdef __x86_64__

// Returns true if the current CPU has this feature enabled.
// We assume supported features do not change dynamically, this function
// may return cached information.
bool HasX86CPUFeature(X86CPUFeatures feature);

#endif  // __x86_64__

}  // namespace silifuzz

#endif  // THIRD_PARTY_SILIFUZZ_UTIL_CPU_FEATURES_H_
