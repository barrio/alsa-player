#include <iostream>
int main(const int argc, const char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: ./myPlayAlsa <file>" << std::endl;
    return 1;
  }
  const std::string filename = argv[1];



  return 0;
}