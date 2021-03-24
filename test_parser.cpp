#include <sstream>
#include "parser.hpp"

int main()
{
  // read the input into a buffer
  std::stringstream input;
  input << std::cin.rdbuf();

  // parse the input
  hattip::lexer lex{input};
  hattip::message msg;
  lex >> msg;

  // regenerate the input
  std::stringstream regenerated_input;
  regenerated_input << msg;

  // assert the regenerated input is identical to the original
  assert(regenerated_input.str() == input.str());

  std::cout << "---Message begins---" << std::endl;
  std::cout << regenerated_input.str();
  std::cout << "---Message ends---" << std::endl;

  std::cout << "OK" << std::endl;

  return 0;
}

