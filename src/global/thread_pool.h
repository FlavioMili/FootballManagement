// -----------------------------------------------------------------------------
//  Football Management Project
//  Copyright (c) 2025 - 2026 Flavio Milinanni. All Rights Reserved.
//
//  This file is part of the Football Management Project.
//  See the LICENSE file in the project root.
// -----------------------------------------------------------------------------

#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

/**
 * @class ThreadPool
 * @brief Small fixed-size pool of background workers for batch work.
 *
 * Workers run below the priority of the calling thread so background
 * simulation never competes with the UI or other applications. parallelFor()
 * lets the calling thread take part in its own batch, so it completes even
 * when every worker is busy and cannot deadlock when called from a worker.
 */
class ThreadPool
{
 public:
  /** Cap on the threads of a batch and on a pool's workers. */
  static constexpr unsigned MAX_THREADS = 8;

  explicit ThreadPool(unsigned worker_count);

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  unsigned getWorkerCount() const
  {
    return static_cast<unsigned>(workers.size());
  }

  /**
   * Runs task(i) for every i in [0, count) on the workers and the calling
   * thread, returning once all calls finished. The first exception thrown by
   * a task is rethrown here after the batch completed.
   */
  void parallelFor(std::size_t count,
                   const std::function<void(std::size_t)>& task);

  /**
   * Threads a batch should use including the caller: FM_SIM_THREADS when set
   * (1 = run on the caller only), otherwise one less than the hardware
   * threads so the UI thread keeps a core; always in [1, MAX_THREADS].
   */
  static unsigned defaultThreadCount();

 private:
  void workerLoop(const std::stop_token& stop);

  std::mutex mutex;
  std::condition_variable_any wake;
  std::deque<std::function<void()>> jobs;
  // Last member: workers stop and join before the queue is destroyed.
  std::vector<std::jthread> workers;
};
