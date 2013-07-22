#include "ReaperCore.hh"

static void printLog()
{
  std::ifstream log("log");
  std::string line;
  while (std::getline(log, line))
    std::cout << line << std::endl;
  log.close();
}

int main(int ac, char **av)
{
  static_cast<void>(ac);
  static_cast<void>(av);

  try
  {
    ReaperCore core;
    core.run();
  }
  catch (const std::exception& e)
  {
    std::cerr << "Unknown exception caught: " << e.what() << std::endl;
  }
  printLog();
}
