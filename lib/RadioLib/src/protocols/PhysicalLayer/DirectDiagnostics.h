#pragma once
#include <atomic>
#include <stdint.h>
// Optional instrumentation only: do not change receive-buffer behavior.
// Counters have one ISR writer; no allocation, clock reads, logging or locks in the ISR.
struct RadioDirectDiagnostics {
  std::atomic<bool> enabled{false};
  std::atomic<uint32_t> syncs{0}, fullDrops{0}, resets{0}, discardedBytes{0}, peak{0};
  void synchronized(uint32_t unread) {
    if (!enabled.load(std::memory_order_relaxed)) return;
    syncs.fetch_add(1, std::memory_order_relaxed);
    if (unread) {
      resets.fetch_add(1, std::memory_order_relaxed);
      discardedBytes.fetch_add(unread, std::memory_order_relaxed);
    }
  }
  void droppedByte() {
    if (enabled.load(std::memory_order_relaxed)) fullDrops.fetch_add(1, std::memory_order_relaxed);
  }
  void completedByte(uint32_t count) {
    if (!enabled.load(std::memory_order_relaxed)) return;
    uint32_t old = peak.load(std::memory_order_relaxed);
    while (old < count && !peak.compare_exchange_weak(old, count, std::memory_order_relaxed)) {}

  }
};
static_assert(ATOMIC_INT_LOCK_FREE == 2, "ISR diagnostics require lock-free counters");
static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "ISR diagnostics require a lock-free enable flag");
