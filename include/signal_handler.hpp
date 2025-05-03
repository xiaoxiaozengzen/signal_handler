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

  /**
   * @brief 单例模式
   */
  static SignalHandler& get_global_signal_handler();

  /**
   * @brief 安装信号处理函数
   */
  bool install();

  /**
   * @brief 卸载信号处理函数
   */
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

  /**
   * @brief 信号处理函数
   * 
   * @note 被安装的信号处理函数调用，用于按照顺序依次处理信号
   */
  static void signal_handler_common();

#if defined(HAS_SIGACTION)
  static void signal_handler(int signal_value, siginfo_t * siginfo, void * context);
#else
  static void signal_handler(int signal_value);
#endif

  /**
   * @brief 工作线程绑定的handler，
   * 
   * @note 没收到信号的时候会阻塞在这里。
   *       收到信号后会解除阻塞，处理完信号后继续阻塞
   */
  void deferred_signal_handler();

  /**
   * @brief 开启对信号的等待
   */
  static void setup_wait_for_signal();

  /**
   * @brief 关闭对信号的等待
   */
  static void teardown_wait_for_signal() noexcept;

  /**
   * @brief 等待，直到接收到信号
   */
  static void wait_for_signal();

  /**
   * @brief 收到信号，解除阻塞
   */
  static void notify_signal_handler() noexcept;

private:
  ///< 是否接收到信号，接收到置true，处理完后置false
  static std::atomic_bool signal_received_;

  ///< 监听信号的线程
  std::thread signal_handler_thread_;

  ///< 安装信号处理函数与否
  std::mutex install_mutex_;
  std::atomic_bool installed_{false};

  ///< 是否等待信号得到来
  static std::atomic_bool wait_for_signal_is_setup_;

  ///< 信号量，用于按顺序处理信号
  static sem_t signal_handler_sem_;
};

}  // namespace cgz

#endif  // SIGNAL_HANDLER_HPP_
