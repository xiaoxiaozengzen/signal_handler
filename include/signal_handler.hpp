#ifndef SIGNAL_HANDLER_HPP_
#define SIGNAL_HANDLER_HPP_

#include <semaphore.h>

#include <atomic>
#include <csignal>
#include <mutex>
#include <thread>

namespace cgz
{
class SignalHandler final
{
public:
  static SignalHandler& get_global_signal_handler();

  bool install();

  bool uninstall();

  bool is_installed();

private:
  SignalHandler() = default;

  ~SignalHandler();

#if defined(HAS_SIGACTION)
  using signal_handler_type = struct sigaction;
#else
  using signal_handler_type = void (*)(int);
#endif

  static SignalHandler::signal_handler_type old_signal_handler_;

  static SignalHandler::signal_handler_type set_signal_handler(int signal_value, const SignalHandler::signal_handler_type & signal_handler);

  static void signal_handler_common();

#if defined(HAS_SIGACTION)
  static void signal_handler(int signal_value, siginfo_t * siginfo, void * context);
#else
  static void signal_handler(int signal_value);
#endif

  void deferred_signal_handler();

  static void setup_wait_for_signal();

  static void teardown_wait_for_signal() noexcept;

  static void wait_for_signal();

  static void notify_signal_handler() noexcept;

private:
  static std::atomic_bool signal_received_;
  std::thread signal_handler_thread_;

  std::mutex install_mutex_;
  std::atomic_bool installed_{false};
  static std::atomic_bool wait_for_signal_is_setup_;

  static sem_t signal_handler_sem_;
};

}  // namespace cgz

#endif  // SIGNAL_HANDLER_HPP_
