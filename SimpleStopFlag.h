#pragma once

#include <atomic>
#include "StopFlag.h"

class SimpleStopFlag final : public StopFlag {
 public:
  SimpleStopFlag() : value(false) {}
  SimpleStopFlag(SimpleStopFlag const&) = delete;
  SimpleStopFlag& operator=(SimpleStopFlag const&) = delete;

  bool IsStop() const override { return value.load(); }

  void Stop() override { value.store(true); }

 private:
  std::atomic<bool> value;
};