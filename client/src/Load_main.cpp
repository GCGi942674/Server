#include "EchoClient.h"
#include <atomic>
#include <iostream>

class LoadStats {
public:
  std::atomic<uint64_t> total_requests{0};
  std::atomic<uint64_t> success_requests{0};
  std::atomic<uint64_t> failed_requests{0};
  std::atomic<uint64_t> total_rtt_us{0};
  std::atomic<uint64_t> max_rtt_us{0};
};

static void updateMax(std::atomic<uint64_t> &target, uint64_t value) {
  uint64_t old = target.load(std::memory_order_relaxed);
  while (value > old &&
         !target.compare_exchange_weak(old, value, std::memory_order_relaxed)) {
  }
}

void workerLoop(const std::string &ip, int port, std::atomic<bool> &stop,
                LoadStats &stats, int worker_id) {
  EchoClient client(ip, port);

  while (!stop.load()) {
    std::string response;
    std::string message = "Hello from worker" + std::to_string(worker_id);

    auto begin = std::chrono::steady_clock::now();
    bool ok = client.sendMessage(message, response);
    auto end = std::chrono::steady_clock::now();

    uint64_t rtt_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
            .count();

    stats.total_requests.fetch_add(1, std::memory_order_relaxed);

    if (ok) {
      stats.success_requests.fetch_add(1, std::memory_order_relaxed);
      stats.total_rtt_us.fetch_add(rtt_us, std::memory_order_relaxed);
      updateMax(stats.max_rtt_us, rtt_us);
    } else {
      stats.failed_requests.fetch_add(1, std::memory_order_relaxed);
      std::this_thread::sleep_for(
          std::chrono::milliseconds(100)); // backoff on failure
    }
  }
  client.disconnect();
}

void reporterLoop(std::atomic<bool> &stop, LoadStats &stats) {
  uint64_t last_total = 0;

  while (!stop.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    uint64_t total = stats.total_requests.load();
    uint64_t success = stats.success_requests.load();
    uint64_t failed = stats.failed_requests.load();
    uint64_t total_rtt = stats.total_rtt_us.load();
    uint64_t max_rtt = stats.max_rtt_us.load();

    uint64_t qps = total - last_total;
    last_total = total;

    uint64_t avg_rtt = success == 0 ? 0 : total_rtt / success;

    std::cout << "[stats] total = " << total << " "
              << "success=" << success << " "
              << "failed=" << failed << " "
              << "qps=" << qps << " "
              << "avg_rtt_us=" << avg_rtt << " "
              << "max_rtt_us=" << max_rtt << std::endl;
  }
}

int main(int argc, char *argv[]) {
  if (argc != 5) {
    std::cerr << "Usage: " << argv[0]
              << " <ip> <port> <connections> <duration_sec>" << std::endl;
    return 1;
  }

  std::string ip = argv[1];
  int port = std::stoi(argv[2]);
  int connections = std::stoi(argv[3]);
  int duration_sec = std::stoi(argv[4]);

  if (connections <= 0 || duration_sec <= 0) {
    std::cerr << "connections and duration_sec must be positive." << std::endl;
    return 1;
  }

  LoadStats stats;
  std::atomic<bool> stop{false};

  std::vector<std::thread> workers;
  workers.reserve(static_cast<size_t>(connections));

  for (int i = 0; i < connections; ++i) {
    workers.emplace_back(workerLoop, ip, port, std::ref(stop), std::ref(stats),
                         i);
  }

  std::thread reporter(reporterLoop, std::ref(stop), std::ref(stats));

  auto start = std::chrono::steady_clock::now();
  std::this_thread::sleep_for(std::chrono::seconds(duration_sec));
  stop.store(true, std::memory_order_relaxed);

  for (auto &t : workers) {
    if (t.joinable()) {
      t.join();
    }
  }

  if (reporter.joinable()) {
    reporter.join();
  }

  auto end = std::chrono::steady_clock::now();
  uint64_t elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count();

  uint64_t total = stats.total_requests.load(std::memory_order_relaxed);
  uint64_t success = stats.success_requests.load(std::memory_order_relaxed);
  uint64_t failed = stats.failed_requests.load(std::memory_order_relaxed);
  uint64_t total_rtt = stats.total_rtt_us.load(std::memory_order_relaxed);
  uint64_t max_rtt = stats.max_rtt_us.load(std::memory_order_relaxed);

  uint64_t avg_rtt = (success == 0) ? 0 : (total_rtt / success);
  double qps = (elapsed_ms == 0) ? 0.0 : (total * 1000.0 / elapsed_ms);

  std::cout << "\n===== Load Test Summary =====" << std::endl;
  std::cout << "elapsed_ms=" << elapsed_ms << std::endl;
  std::cout << "total=" << total << std::endl;
  std::cout << "success=" << success << std::endl;
  std::cout << "failed=" << failed << std::endl;
  std::cout << "avg_rtt_us=" << avg_rtt << std::endl;
  std::cout << "max_rtt_us=" << max_rtt << std::endl;
  std::cout << "avg_qps=" << qps << std::endl;

  return 0;
}