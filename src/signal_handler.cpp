#include <semaphore.h>

#include <atomic>
#include <csignal>
#include <mutex>
#include <string>
#include <thread>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <memory>

#include "signal_handler.hpp"


void get_logger()
{

}

#define RCLCPP_DEBUG(logger, msg, ...) \
  printf("DEBUG: " msg "\n", ##__VA_ARGS__)

#define RCLCPP_INFO(logger, msg, ...) \
  printf("INFO: " msg "\n", ##__VA_ARGS__)

#define RCLCPP_ERROR(logger, msg, ...) \
  printf("ERROR: " msg "\n", ##__VA_ARGS__)

namespace cgz
{

SignalHandler::signal_handler_type SignalHandler::old_signal_handler_;
std::atomic_bool SignalHandler::signal_received_ = {false};
std::atomic_bool SignalHandler::wait_for_signal_is_setup_ = {false};
sem_t SignalHandler::signal_handler_sem_;

SignalHandler & SignalHandler::get_global_signal_handler()
{
  static SignalHandler signal_handler;
  return signal_handler;
}

bool SignalHandler::install()
{
  std::lock_guard<std::mutex> lock(install_mutex_);
  bool already_installed = installed_.exchange(true);
  if (already_installed) {
    return false;
  }
  try {
    setup_wait_for_signal();
    signal_received_.store(false);

    SignalHandler::signal_handler_type signal_handler_argument;
#if defined(HAS_SIGACTION)
    memset(&signal_handler_argument, 0, sizeof(signal_handler_argument));
    sigemptyset(&signal_handler_argument.sa_mask);
    signal_handler_argument.sa_sigaction = signal_handler;
    signal_handler_argument.sa_flags = SA_SIGINFO;
#else
    signal_handler_argument = signal_handler;
#endif

    old_signal_handler_ = SignalHandler::set_signal_handler(SIGINT, signal_handler_argument);

    signal_handler_thread_ = std::thread(&SignalHandler::deferred_signal_handler, this);
  } catch (...) {
    installed_.store(false);
    throw;
  }
  RCLCPP_DEBUG(get_logger(),"signal handler installed");
  return true;
}

bool SignalHandler::uninstall()
{
  std::lock_guard<std::mutex> lock(install_mutex_);
  bool installed = installed_.exchange(false);
  if (!installed) {
    return false;
  }
  try {
    set_signal_handler(SIGINT, old_signal_handler_);
    RCLCPP_DEBUG(get_logger(), "SignalHandler::uninstall(): notifying deferred signal handler");
    notify_signal_handler();
    signal_handler_thread_.join();
    teardown_wait_for_signal();
  } catch (...) {
    installed_.exchange(true);
    throw std::runtime_error(
      "SignalHandler::uninstall(): exception while uninstalling signal handler");
  }
  RCLCPP_DEBUG(get_logger(), "signal handler uninstalled");
  return true;
}

bool SignalHandler::is_installed()
{
  return installed_.load();
}

SignalHandler::~SignalHandler()
{
  try {
    uninstall();
  } catch (const std::exception & exc) {
    RCLCPP_ERROR(
      get_logger(),
      "caught %s exception when uninstalling the sigint handler in rclcpp::~SignalHandler: %s",
      "deconstructor", exc.what());
  } catch (...) {
    RCLCPP_ERROR(
      get_logger(),
      "caught unknown exception when uninstalling the sigint handler in rclcpp::~SignalHandler");
  }
}

SignalHandler::signal_handler_type SignalHandler::set_signal_handler(int signal_value, const SignalHandler::signal_handler_type & signal_handler)
{
  bool signal_handler_install_failed;
  SignalHandler::signal_handler_type old_signal_handler;
#if defined(HAS_SIGACTION)
  ssize_t ret = sigaction(signal_value, &signal_handler, &old_signal_handler);
  signal_handler_install_failed = (ret == -1);
#else
  old_signal_handler = std::signal(signal_value, signal_handler);
  signal_handler_install_failed = (old_signal_handler == SIG_ERR);
#endif
  if (signal_handler_install_failed) {
    char error_string[1024] = {'a'};
    
    auto msg =
      "Failed to set SIGINT signal handler (" + std::to_string(errno) + "): " + error_string;
    throw std::runtime_error(msg);
  }

  return old_signal_handler;
}

void SignalHandler::signal_handler_common()
{
  signal_received_.store(true);
  RCLCPP_DEBUG(
    get_logger(),
    "signal_handler(): SIGINT received, notifying deferred signal handler");
  notify_signal_handler();
}

#if defined(HAS_SIGACTION)
void SignalHandler::signal_handler(int signal_value, siginfo_t * siginfo, void * context)
{
  RCLCPP_INFO(get_logger(), "signal_handler(signal_value=%d)", signal_value);

  if (old_signal_handler_.sa_flags & SA_SIGINFO) {
    if (old_signal_handler_.sa_sigaction != NULL) {
      old_signal_handler_.sa_sigaction(signal_value, siginfo, context);
    }
  } else {
    if (
      old_signal_handler_.sa_handler != NULL &&  // Is set
      old_signal_handler_.sa_handler != SIG_DFL &&  // Is not default
      old_signal_handler_.sa_handler != SIG_IGN)  // Is not ignored
    {
      old_signal_handler_.sa_handler(signal_value);
    }
  }

  signal_handler_common();
}
#else
void SignalHandler::signal_handler(int signal_value)
{
  RCLCPP_INFO(get_logger(), "signal_handler(signal_value=%d)", signal_value);

  if (old_signal_handler_) {
    old_signal_handler_(signal_value);
  }

  signal_handler_common();
}
#endif

void SignalHandler::deferred_signal_handler()
{
  while (true) {
    if (signal_received_.exchange(false)) {
      RCLCPP_DEBUG(get_logger(), "deferred_signal_handler(): SIGINT received, shutting down");
    }
    if (!is_installed()) {
      RCLCPP_DEBUG(get_logger(), "deferred_signal_handler(): signal handling uninstalled");
      break;
    }
    RCLCPP_DEBUG(get_logger(), "deferred_signal_handler(): waiting for SIGINT or uninstall");
    wait_for_signal();
    RCLCPP_DEBUG(get_logger(), "deferred_signal_handler(): woken up due to SIGINT or uninstall");
  }
}

void SignalHandler::setup_wait_for_signal()
{
  if (-1 == sem_init(&signal_handler_sem_, 0, 0)) {
    throw std::runtime_error(std::string("sem_init() failed: ") + strerror(errno));
  }

  wait_for_signal_is_setup_.store(true);
}

void SignalHandler::teardown_wait_for_signal() noexcept
{
  if (!wait_for_signal_is_setup_.exchange(false)) {
    return;
  }

  if (-1 == sem_destroy(&signal_handler_sem_)) {
    RCLCPP_ERROR(get_logger(), "invalid semaphore in teardown_wait_for_signal()");
  }

}

void SignalHandler::wait_for_signal()
{
  if (!wait_for_signal_is_setup_.load()) {
    RCLCPP_ERROR(get_logger(), "called wait_for_signal() before setup_wait_for_signal()");
    return;
  }

  int s;
  do {
    s = sem_wait(&signal_handler_sem_);
  } while (-1 == s && EINTR == errno);
}

void SignalHandler::notify_signal_handler() noexcept
{
  if (!wait_for_signal_is_setup_.load()) {
    return;
  }

  if (-1 == sem_post(&signal_handler_sem_)) {
    RCLCPP_ERROR(get_logger(), "sem_post failed in notify_signal_handler()");
  }
}

}  // namespace cgz
