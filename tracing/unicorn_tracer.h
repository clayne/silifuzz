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

#ifndef THIRD_PARTY_SILIFUZZ_TRACING_UNICORN_TRACER_H_
#define THIRD_PARTY_SILIFUZZ_TRACING_UNICORN_TRACER_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>

#include "absl/crc/crc32c.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "./common/proxy_config.h"
#include "./common/raw_insns_util.h"
#include "./common/snapshot.h"
#include "./common/snapshot_util.h"
#include "./tracing/unicorn_util.h"
#include "./util/arch.h"
#include "./util/checks.h"
#include "./util/itoa.h"
#include "./util/ucontext/ucontext_types.h"
#include "third_party/unicorn/unicorn.h"

namespace silifuzz {

template <typename Arch>
struct UnicornTracerConfig;

template <>
struct UnicornTracerConfig<X86_64> {};

template <>
struct UnicornTracerConfig<AArch64> {
  // Force Unicorn to emulate a A72 processor.
  // This is a bit old, but if used as a fuzz target the resulting tests should
  // be compatible with most hardware you would want to run on.
  // By default Unicorn is roughly a A77+, it support sha512, sm3, and sm4.
  bool force_a72 = false;
};

// An architecture-generic class for executing code snippets in Unicorn.
// This class is not thread safe.  Each thread should have its own instance.
// Note: this header includes some non-trivial functions. This isn't ideal, but
// since this a templated class, moving them out of line requires explicitly
// instantiating them for each arch. Explicitly instantiating them for each arch
// has consequences for the linker, particularly because we're trying to let the
// user link in only the arch support they need to simplify coverage reports.
template <typename Arch>
class UnicornTracer {
 public:
  UnicornTracer() : uc_(nullptr) {}
  ~UnicornTracer() { Destroy(); }

  void Destroy() {
    if (uc_ != nullptr) {
      uc_close(uc_);
      uc_ = nullptr;
    }
  }

  // Prepare Unicorn to run a code snippet.
  absl::Status InitSnippet(absl::string_view instructions,
                           const UnicornTracerConfig<Arch>& tracer_config =
                               UnicornTracerConfig<Arch>{},
                           const FuzzingConfig<Arch>& fuzzing_config =
                               DEFAULT_FUZZING_CONFIG<Arch>) {
    ASSIGN_OR_RETURN_IF_NOT_OK(
        Snapshot snapshot,
        InstructionsToSnapshot<Arch>(instructions, fuzzing_config));

    UContext<Arch> ucontext;
    absl::Status status = ConvertRegsFromSnapshot(
        snapshot.registers(), &ucontext.gregs, &ucontext.fpregs);
    if (!status.ok()) {
      LOG_FATAL("Failed to deserialize registers - ", status.message());
    }

    InitUnicorn(tracer_config);

    SetupSnippetMemory(snapshot, ucontext, fuzzing_config);

    SetInitialRegisters(ucontext);

    code_start_address_ = GetInstructionPointer();
    code_end_address_ = GetExitPoint(snapshot);

    // Hook instruction execution so that we can always count the number of
    // instructions executed. This is what Unicorn does internally when you try
    // to limit the number of instructions executed. Doing it outside Unicorn
    // allows us to know when we hit the limit.
    UNICORN_CHECK(uc_hook_add(uc_, &hook_code_, UC_HOOK_CODE,
                              (void*)&DispatchHookCode, this, 1, 0));

    return absl::OkStatus();
  }

  using UserCallback = void(UnicornTracer<Arch>& tracer);

  // Callback setters. F should be compatible with UserCallback.
  // The callback setters should not be called more than once.
  // Invoked before the code snippet is executed.
  template <typename F>
  void SetBeforeExecutionCallback(F&& callback) {
    CHECK(!before_execution_callback_);
    before_execution_callback_ = callback;
  }
  // Invoked before each instruction is executed.
  template <typename F>
  void SetBeforeInstructionCallback(F&& callback) {
    CHECK(!before_instruction_callback_);
    before_instruction_callback_ = callback;
  }
  // Invoked after the code snippet is executed.
  template <typename F>
  void SetAfterExecutionCallback(F&& callback) {
    CHECK(!after_execution_callback_);
    after_execution_callback_ = callback;
  }

  // Run the code snippet. Execution will stop after `max_insn_executed`
  // instructions to help avoid infinite loops.
  absl::Status Run(size_t max_insn_executed) {
    num_instructions_ = 0;
    max_instructions_ = max_insn_executed;
    should_be_stopped_ = false;

    BeforeExecution();

    // Unicorn can hang due to bugs in QEMU.
    // Halt execution if it exceeds 1 seconds of wall clock time.
    // This value is arbitrary and may need to be tuned.
    // We don't want this value to be so small that machine load can easily
    // cause the deadline to be missed.
    // We don't want this value to be so large that fault injection will take
    // forever when we hit a degenerate case.
    // Empirically, 1 second is about 20x-30x longer than execution takes in the
    // worst case on an unloaded machine.
    uint64_t timeout_microseconds = 1000000;
    uc_err err = uc_emu_start(uc_, code_start_address_, code_end_address_,
                              timeout_microseconds, 0);
    AfterExecution();

    // Check if the emulator stopped cleanly.
    if (err) {
      return absl::InternalError(absl::StrCat(
          "uc_emu_start() returned ", IntStr(err), ": ", uc_strerror(err)));
    }

    // We only stop emulation when we see more instructions than the limit.
    // Exactly at the limit is not an error.
    if (num_instructions_ > max_instructions_) {
      return absl::InternalError("emulator executed too many instructions");
    }

    // Check if the timeout fired.
    size_t result;
    UNICORN_CHECK(uc_query(uc_, UC_QUERY_TIMEOUT, &result));
    if (result) {
      return absl::InternalError("execution timed out");
    }

    // Check if the emulator stopped at the right address.
    // Generally, this should not be an issue if we did not hit the instruction
    // count limit or the time limit.
    uint64_t pc = GetInstructionPointer();
    if (pc != code_end_address_) {
      return absl::InternalError("execution did not reach end of code snippet");
    }

    RETURN_IF_NOT_OK(ValidateArchEndState());

    return absl::OkStatus();
  }

  // Should only be invoked inside callbacks from Run()
  void Stop() {
    uc_emu_stop(uc_);
    should_be_stopped_ = true;
  }

  // Checksum some of the tracer's mutable memory.
  // This function does not checksum all the mutable memory because this can be
  // quite slow for the x86_64 which has ~1GB of mutable memory.
  // This checksum can be used by fault injection to quickly estimate if the end
  // state is different than expected.
  // The exact definition of this checksum may change over time, comparing a
  // value produced by an old version of the software against a value produced
  // by a new version of the software is not meaningful.
  uint32_t PartialChecksumOfMutableMemory() {
    uint32_t count;
    uc_mem_region* regions;
    UNICORN_CHECK(uc_mem_regions(uc_, &regions, &count));
    absl::crc32c_t checksum(0);
    char data[4096];
    for (uint32_t i = 0; i < count; ++i) {
      if (regions[i].perms & UC_PROT_WRITE) {
        // Empirically, checksumming the first 8 pages of each mutable region
        // covers ~58% of the pages the proxy tends to dirty. Doubling this
        // raises the coverage to 59%. Beyond the first few pages, the access
        // patterns are fairly unpredictable, so we'd need to checksum vastly
        // more memory to catch all the dirty pages. Unfortunately checksumming
        // all the mutable memory on x86_64 would make fault injection ~18x
        // slower. So we're trading some accuracy for a huge amount of speed.
        uint64_t end_offset =
            std::min(8 * 4096UL, regions[i].end - regions[i].begin + 1);
        CHECK_EQ(end_offset % sizeof(data), 0);
        for (uint64_t offset = 0; offset < end_offset; offset += sizeof(data)) {
          // Note: if we originally mapped this memory with uc_mem_map_ptr then
          // we could read it directly and avoid a copy. For a small number of
          // pages, however, this doesn't offer a huge performance improvement.
          UNICORN_CHECK(
              uc_mem_read(uc_, regions[i].begin + offset, data, sizeof(data)));
          checksum = absl::ExtendCrc32c(checksum,
                                        absl::string_view(data, sizeof(data)));
        }
      }
    }
    UNICORN_CHECK(uc_free(regions));
    return static_cast<uint32_t>(checksum);
  }

  uint64_t GetInstructionPointer();
  void SetInstructionPointer(uint64_t address);

  uint64_t GetStackPointer();

  // Read the current register state. Not all platforms can read all registers,
  // so some registers may be set to zero instead of their actual values.
  void GetRegisters(UContext<Arch>& ucontext);

  // Write the current register state. Not all platforms can write all
  // registers, so some registers may not be updated.
  void SetRegisters(const UContext<Arch>& ucontext);

  void ReadMemory(uint64_t address, void* buffer, size_t size) {
    UNICORN_CHECK(uc_mem_read(uc_, address, buffer, size));
  }

  // HACK so X86_64 can check it isn't executing an instruction that dangles
  // past the end of code. This can happens if the fuzzing input ends with a
  // partial instruction that depends on the bytes that come after the test.
  bool InstructionIsInRange(uint64_t address, size_t size) const {
    return address >= code_start_address_ &&
           address + size <= code_end_address_;
  }

  uint64_t GetCodeStartAddress() const { return code_start_address_; }

 private:
  // Initialize Unicorn and put it in a state that it can execute code snippets
  // and Snapshots. This may involve setting system registers, etc.
  void InitUnicorn(const UnicornTracerConfig<Arch>& tracer_config);

  // Create a memory mapping or die. Helps avoid error handling in the cases we
  // know should succeed unless there is a bug.
  void MapMemory(uint64_t addr, uint64_t size, uint32_t prot) {
    uc_err err = uc_mem_map(uc_, addr, size, prot);
    if (err != UC_ERR_OK) {
      LOG_FATAL("mapping ", HexStr(addr), " + ", HexStr(size), " failed with ",
                IntStr(err), ": ", uc_strerror(err));
    }
  }

  // Setup the memory mappings and memory contents for a snippet that has been
  // turned into a Snapshot with InstructionsToSnapshot.
  void SetupSnippetMemory(const Snapshot& snapshot,
                          const UContext<Arch>& ucontext,
                          const FuzzingConfig<Arch>& fuzzing_config);

  // Set Unicorn's architectural state. The Unicorn API may not give access to
  // setting all the state that we want, so this function may execute arbitrary
  // instructions. For this reason, the method will only be called during init.
  void SetInitialRegisters(const UContext<Arch>& ucontext);

  // Check that the arch-specific architectural state is acceptable after the
  // instructions are done executing. Unicorn may not catch all this issues that
  // will prevent turning this into a valid Snapshot.
  absl::Status ValidateArchEndState();

  void HookCode(uint64_t address, uint32_t size) {
    if (num_instructions_ >= max_instructions_) {
      // QEMU x86_64 may not always respect uc_emu_stop().
      // Similar to Unicorn, we'll call stop repeatedly once the limit has been
      // passed.
      // We don't invoke the callback in this case so that clients can behave
      // as if the limit works.
      Stop();
    } else if (!should_be_stopped_) {
      // If Stop() has been called, we suppress further callbacks. Unicorn may
      // not stop immediately.
      BeforeInstruction();
    }
    num_instructions_++;
  }

  static void DispatchHookCode(uc_engine* uc, uint64_t address, uint32_t size,
                               void* user_data) {
    UnicornTracer<Arch>* tracer = static_cast<UnicornTracer<Arch>*>(user_data);
    tracer->HookCode(address, size);
  }

  // Callback invocation
  void BeforeExecution() {
    if (before_execution_callback_) {
      before_execution_callback_(*this);
    }
  }
  void BeforeInstruction() {
    if (before_instruction_callback_) {
      before_instruction_callback_(*this);
    }
  }
  void AfterExecution() {
    if (after_execution_callback_) {
      after_execution_callback_(*this);
    }
  }

  uc_engine* uc_;

  uint64_t code_start_address_;
  uint64_t code_end_address_;

  uc_hook hook_code_;

  size_t num_instructions_;
  size_t max_instructions_;
  bool should_be_stopped_;

  std::function<UserCallback> before_execution_callback_;
  std::function<UserCallback> before_instruction_callback_;
  std::function<UserCallback> after_execution_callback_;
};

}  // namespace silifuzz

#endif  // THIRD_PARTY_SILIFUZZ_TRACING_UNICORN_TRACER_H_
