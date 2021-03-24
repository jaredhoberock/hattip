#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <iostream>
#include <optional>
#include <set>
#include <variant>
#include <vector>


namespace hattip
{


inline bool is_number(const std::string& s)
{
  return !s.empty() and std::all_of(s.begin(), s.end(), [](char c){ return std::isdigit(c); });
}


template<class T>
inline bool contains(const std::set<T>& s, const T& x)
{
  return s.find(x) != s.end();
}


std::string show_specials(const std::string& s)
{
  std::string result = s;

  if(result == " ")
  {
    result = "<SP>";
  }
  else if(result == "\r")
  {
    result = "<CR>";
  }
  else if(result == "\n")
  {
    result = "<LF>";
  }

  return result;
}


const std::set<int> known_status_codes = {
  100,
  101,
  200,
  201,
  203,
  204,
  205,
  206,
  300,
  301,
  302,
  303,
  304,
  305,
  400,
  401,
  402,
  403,
  404,
  405,
  406,
  407,
  408,
  409,
  410,
  411,
  412,
  500,
  501,
  502,
  503,
  504
};


const std::set<char> tspecials = {
  '(',
  ')',
  '<',
  '>',
  '@',
  ',',
  ';',
  ':',
  '\\',
  '\"',
  '/',
  '[',
  ']',
  '?',
  '=',
  '{',
  '}',
  ' ',
  9    // horizontal-tab
};


inline bool is_tspecial(char ch)
{
  return contains(tspecials, ch);
}

inline bool is_tspecial(const std::string& s)
{
  return s.size() == 1 and is_tspecial(s.front());
}


inline bool is_ctl(char ch)
{
  return (0 <= ch and ch <= 31) or (ch == 127);
}


inline bool is_ctl(const std::string& s)
{
  return s.size() == 1 and is_ctl(s.front());
}


struct lexer
{
  inline lexer(std::istream& input)
    : current_token_{}, input_{input}
  {
    next();
  }

  inline lexer& operator>>(std::string& s)
  {
    s = current_token_;
    next();
    return *this;
  }

  inline lexer& operator>>(int& number)
  {
    std::string tmp;
    *this >> tmp;
    number = std::stoi(tmp);

    return *this;
  }

  inline lexer& operator>>(const char* literal)
  {
    if(literal == current_token_)
    {
      next();
    }
    else
    {
      auto what = std::string{"Expected \""};

      if(std::strcmp(literal, "\r") == 0)
      {
        what += "<CR>";
      }
      else if(std::strcmp(literal, "\n") == 0)
      {
        what += "<LF>";
      }
      else
      {
        what += literal;
      }
      
      what += "\"";

      throw std::runtime_error{what};
    }

    return *this;
  }

  inline std::string next()
  {
    std::string result = current_token_;

    char ch = input_.peek();
    if(ch == std::istream::traits_type::eof())
    {
      // first look for EOF

      current_token_.clear();
    }
    else if(ch == ' ' or ch == '\r' or ch == '\n')
    {
      // look for a space, CR, or LF

      current_token_ = input_.get();
    }
    else if(is_tspecial(ch))
    {
      // look for a tspecial

      current_token_ = input_.get();
    }
    else if(std::isdigit(ch))
    {
      // look for a number

      current_token_.clear();

      while(std::isdigit(ch))
      {
        current_token_.push_back(ch);
        input_.get();
        ch = input_.peek();
      }
    }
    else if(std::isalpha(ch))
    {
      // look for a word
      
      current_token_.clear();

      while(std::isalpha(ch))
      {
        current_token_.push_back(ch);
        input_.get();
        ch = input_.peek();
      }
    }
    else
    {
      // by default, just return the single character

      current_token_ = ch;
      input_.get();
    }

    return result;
  }

  const std::string& peek() const
  {
    return current_token_;
  }

  std::string current_token_;
  std::istream &input_;
};


void expect(const char* expected, lexer& lex)
{
  if(lex.peek() != expected)
  {
    auto what = std::string{"Expected \""} + expected + "\"";
    throw std::runtime_error{what};
  }

  lex.next();
}


struct request_uri : public std::string
{
  using std::string::string;

  // Request-URI := "*" | absoluteURI | abs_path
  // XXX for now, just accept any string not containing whitespace
  friend lexer& operator>>(lexer& lex, request_uri& self)
  {
    return lex >> static_cast<std::string&>(self);
  }

  friend std::ostream& operator<<(std::ostream& os, const request_uri& self)
  {
    return os << static_cast<const std::string&>(self);
  }
};


struct simple_request
{
  request_uri uri;

  // Simple-Request := "GET" SP Request-URI CRLF
  friend lexer& operator>>(lexer& lex, simple_request& self)
  {
    return lex >> "GET" >> " " >> self.uri >> "\r" >> "\n";
  }

  friend std::ostream& operator<<(std::ostream& os, const simple_request& self)
  {
    return os << "GET" << " " << self.uri << "\r" << "\n";
  }

  static constexpr const char* leftmost_token = "GET";
};


struct http_version
{
  int major;
  int minor;

  static constexpr const char* leftmost_token = "HTTP";

  // HTTP-Version := "HTTP" "/" 1*DIGIT "." 1*DIGIT
  friend lexer& operator>>(lexer& lex, http_version& self)
  {
    return lex >> "HTTP" >> "/" >> self.major >> "." >> self.minor;
  }

  friend std::ostream& operator<<(std::ostream& os, const http_version& self)
  {
    return os << "HTTP" << "/" << self.major << "." << self.minor;
  }
};


struct status_code
{
  int number;

  // Status-Code := <one of the known status codes> | three digit number
  friend lexer& operator>>(lexer& lex, status_code& self)
  {
    std::size_t num_digits = lex.peek().size();

    lex >> self.number;

    if(!contains(known_status_codes, self.number) or num_digits != 3)
    {
      throw std::runtime_error{"Unexpected status code number"};
    }

    return lex;
  }

  friend std::ostream& operator<<(std::ostream& os, const status_code& self)
  {
    return os << self.number;
  }
};


struct reason_phrase : std::string
{
  // Reason-Phrase := *<TEXT, excluding CR, LF>
  friend lexer& operator>>(lexer& lex, reason_phrase& self)
  {
    // slurp text until we encounter a CR or LF
    std::string tmp;
    while(lex.peek() != "\r" and lex.peek() != "\n")
    {
      lex >> tmp;
      self += tmp;
    }

    return lex;
  }
};


struct status_line
{
  http_version version;
  status_code code;
  reason_phrase reason;

  static constexpr const char* leftmost_token = http_version::leftmost_token;

  // Status-Line := HTTP-Version SP Status-Code SP Reason-Phrase CRLF
  friend lexer& operator>>(lexer& lex, status_line& self)
  {
    return lex >> self.version >> " " >> self.code >> " " >> self.reason >> "\r" >> "\n";
  }

  friend std::ostream& operator<<(std::ostream& os, const status_line& self)
  {
    return os << self.version << " " << self.code << " " << self.reason << "\r" << "\n";
  }
};


struct token : std::string
{
  // token := 1*<any CHAR except CTLs or tspecials>
  friend lexer& operator>>(lexer& lex, token& self)
  {
    // slurp text until we encounter a CTL or tspecial
    std::string tmp;
    while(not is_ctl(lex.peek()) and not is_tspecial(lex.peek()))
    {
      lex >> tmp;
      self += tmp;
    }

    if(self.empty())
    {
      throw std::runtime_error{"token: Expected at least one CHAR"};
    }

    return lex;
  }
};


using field_name = token;


struct http_header
{
  field_name name;
  std::string value;

  // HTTP-header := field-name ":" [ field-value ] CRLF
  friend lexer& operator>>(lexer& lex, http_header& self)
  {
    lex >> self.name >> ":";

    // consume text until we encounter carriage return
    std::string tmp;
    while(lex.peek() != "\r")
    {
      lex >> tmp;
      self.value += tmp;
    }

    return lex >> "\r" >> "\n";
  }

  friend std::ostream& operator<<(std::ostream& os, const http_header& self)
  {
    return os << self.name << ":" << self.value << "\r" << "\n";
  }
};


struct simple_response {};
struct full_request {};

struct entity_body : std::string
{
  // Entity-Body := *OCTET
  friend lexer& operator>>(lexer& lex, entity_body& self)
  {
    // consume input until eof
    std::string tmp;
    while(not lex.peek().empty())
    {
      lex >> tmp;
      self += tmp;
    }

    return lex;
  }
};


struct full_response 
{
  status_line sl;
  std::vector<http_header> headers;
  std::optional<entity_body> body;

  static constexpr const char* leftmost_token = status_line::leftmost_token;

  // Full-Response := Status-Line
  //                  *( General-Header
  //                   | Reponse-Header
  //                   | Entity-Header )
  //                  CRLF
  //                  [ Entity-Body ]
  friend lexer& operator>>(lexer& lex, full_response& self)
  {
    lex >> self.sl;

    // read headers until we encounter a carriage return
    while(lex.peek() != "\r")
    {
      self.headers.push_back({});
      lex >> self.headers.back();
    }

    lex >> "\r" >> "\n";

    if(!lex.peek().empty())
    {
      self.body.emplace();
      lex >> *self.body;
    }

    return lex;
  }


  friend std::ostream& operator<<(std::ostream& os, const full_response& self)
  {
    os << self.sl;

    for(const auto& header : self.headers)
    {
      os << header;
    }

    os << "\r\n";

    if(self.body)
    {
      os << *self.body;
    }

    return os;
  }
};


struct message
{
  //std::variant<simple_request, simple_response, full_request, full_response> body;
  std::variant<simple_request, full_response> body;

  // message := Simple-Request | Simple-Response | Full-Request | Full-Response
  friend lexer& operator>>(lexer& lex, message& self)
  {
    if(lex.peek() == simple_request::leftmost_token)
    {
      simple_request sr;
      lex >> sr;
      self.body = sr;
    }
    else if(lex.peek() == full_response::leftmost_token)
    {
      full_response fr;
      lex >> fr;
      self.body = fr;
    }
    else
    {
      throw std::runtime_error{"Unexpected input"};
    }

    return lex;
  }

  friend std::ostream& operator<<(std::ostream& os, const message& self)
  {
    std::visit([&os](const auto& body) mutable
    {
      os << body;
    }, self.body);

    return os;
  }
};


} // end hattip

