#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

#include "signal_handler.hpp"

int main() {
  cgz::SignalHandler & signal_handler = cgz::SignalHandler::get_global_signal_handler();
  if (signal_handler.install()) {
    std::cout << "Signal handler installed successfully." << std::endl;
  } else {
    std::cout << "Failed to install signal handler." << std::endl;
  }


  int count = 0;
  while (count < 5) {
    std::cout << "Running... (" << count << ")" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ++count;
  }

  if (signal_handler.uninstall()) {
    std::cout << "Signal handler uninstalled successfully." << std::endl;
  } else {
    std::cout << "Failed to uninstall signal handler." << std::endl;
  }

  return 0;
}