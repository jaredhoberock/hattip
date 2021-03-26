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


inline bool is_known_status_code(int c)
{
  return contains(known_status_codes, c);
}


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


const std::set<const char*> known_methods = {
  "\"OPTIONS\"",
  "\"GET\"",
  "\"HEAD\"",
  "\"POST\"",
  "\"PUT\"",
  "\"PATCH\"",
  "\"COPY\"",
  "\"MOVE\"",
  "\"DELETE\"",
  "\"LINK\"",
  "\"UNLINK\"",
  "\"TRACE\"",
  "\"WRAPPED\"",
};


inline bool is_known_method(const char* s)
{
  return contains(known_methods, s);
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
};


// LWS := [CRLF] 1*( SP | HT )
inline bool is_lws(const std::string& s)
{
  int i = 0;

  // skip past optional CRLF
  if(s.size() >= 2)
  {
    if(s[i] == '\r' and s[i] == '\n')
    {
      i += 2;
    }
  }

  // neither an empty string nor CRLF alone is lws
  // there needs to be at least one character of white space
  if(i == s.size())
  {
    return false;
  }

  for(; i != s.size(); ++i)
  {
    // any character besides SP or HT is not LWS
    // horizontal tab is ASCII 9
    if(s[i] != ' ' or s[i] != 9)
    {
      return false;
    }
  }

  return true;
}


// qdtext := <any CHAR except <"> and CTLs, but including LWS>
inline bool is_qdtext(const std::string& s)
{
  if(is_lws(s)) return true;

  if(s == "\"") return false;

  if(is_ctl(s)) return false;

  return true;
}


struct quoted_string : std::string
{
  // Quoted-String := <"> *(qdtext) <">
  // qdtext := <any CHAR except <"> and CTLs, but including LWS>
  friend lexer& operator>>(lexer& lex, quoted_string& self)
  {
    // open quote
    lex >> "\"";
    self += "\"";

    // consume text until we encounter another "
    std::string tmp;
    while(lex.peek() != "\"")
    {
      if(!is_qdtext(lex.peek()))
      {
        throw std::runtime_error{"Expected qdtext"};
      }

      lex >> tmp;
      self += tmp;
    }

    // close quote
    lex >> "\"";
    self += "\"";

    return lex;
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


struct method : std::string
{
  // Method := <one of the known methods> | token
  friend lexer& operator>>(lexer& lex, method& self)
  {
    if(lex.peek() == "\"")
    {
      quoted_string qs;
      lex >> qs;

      self.clear();
      self += qs;
    }
    else
    {
      token extension_method_name;
      lex >> extension_method_name;

      self.clear();
      self += extension_method_name;
    }

    return lex;
  }
};


struct http_version
{
  int major;
  int minor;

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


struct request_line
{
  method m;
  request_uri uri;
  http_version version;

  // Request-Line := Method SP Request-URI SP HTTP-Version CRLF
  friend lexer& operator>>(lexer& lex, request_line& self)
  {
    return lex >> self.m >> " " >> self.uri >> " " >> self.version >> "\r" >> "\n";
  }

  friend std::ostream& operator<<(std::ostream& os, const request_line& self)
  {
    return os << self.m << " " << self.uri << " " << self.version << "\r\n";
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


struct http_headers
{
  std::vector<http_header> body;

  // HTTP-Headers := *( General-Header
  //                  | Request-Header
  //                  | Entity-Header )
  //                  CRLF
  friend lexer& operator>>(lexer& lex, http_headers& self)
  {
    // read headers until we encounter a carriage return
    while(lex.peek() != "\r")
    {
      self.body.push_back({});
      lex >> self.body.back();
    }

    lex >> "\r" >> "\n";

    return lex;
  }

  friend std::ostream& operator<<(std::ostream& os, const http_headers& self)
  {
    for(const auto& header : self.body)
    {
      os << header;
    }

    return os << "\r\n";
  }
};


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


struct full_request
{
  request_line rl;
  http_headers headers;
  entity_body body;

  // Full-Request := Request-Line
  //                 HTTP-Headers
  //                 [ Entity-Body ]
  friend lexer& operator>>(lexer& lex, full_request& self)
  {
    return lex >> self.rl >> self.headers >> self.body;
  }

  friend std::ostream& operator<<(std::ostream& os, const full_request& self)
  {
    return os << self.rl << self.headers << self.body;
  }
};


struct request
{
  std::variant<simple_request, full_request> body;

  // Request := Simple-Request | Full-Request
  friend lexer& operator>>(lexer& lex, request& self)
  {
    method m;
    request_uri uri;

    // the first four tokens of simple & full request are the same
    lex >> m >> " " >> uri >> " ";

    // if HTTP-Version comes next, it's a Full-Request
    if(lex.peek() == "HTTP")
    {
      // read the HTTP-Version
      http_version version;
      lex >> version;

      // assemble the Request-Line
      request_line rl{m, uri, version};

      // read the HTTP-Headers and Entity-Body
      http_headers headers;
      entity_body eb;
      lex >> headers >> eb;

      self.body = full_request{rl, headers, eb};
    }
    else
    {
      // else, CRLF must come next and it's a Simple-Request
      // and method must be "GET"
      lex >> "\r" >> "\n";
      if(m != "\"GET\"")
      {
        throw std::runtime_error{"Expected \"GET\""};
      }

      self.body = simple_request{uri};
    }

    return lex;
  }

  friend std::ostream& operator<<(std::ostream& os, const request& self)
  {
    std::visit([&os](const auto& body) mutable
    {
      os << body;
    }, self.body);

    return os;
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

    if(!is_known_status_code(self.number) or num_digits != 3)
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


// Simple-Response := [ Entity-Body ]
// The optional [ ] part is redundant with Entity-Body because Entity-Body is allowed to be empty
struct simple_response : entity_body {};


struct full_response 
{
  status_line sl;
  http_headers headers;
  entity_body body;

  // Full-Response := Status-Line
  //                  HTTP-Headers
  //                  [ Entity-Body ]
  friend lexer& operator>>(lexer& lex, full_response& self)
  {
    return lex >> self.sl >> self.headers >> self.body;
  }


  friend std::ostream& operator<<(std::ostream& os, const full_response& self)
  {
    return os << self.sl << self.headers << self.body;
  }
};


struct message
{
  std::variant<full_response, request, simple_response> body;

  // the spec defines it as:
  // Message := Simple-Request | Simple-Response | Full-Request | Full-Response
  //
  // we implement it here as:
  // Message := Full-Reponse | Request | Simple-Response
  friend lexer& operator>>(lexer& lex, message& self)
  {
    if(lex.peek() == "HTTP")
    {
      full_response fr;
      lex >> fr;
      self.body = fr;
    }
    else if(!lex.peek().empty())
    {
      // Simple-Response is allowed to be completely empty
      //
      // XXX this isn't quite right because we could predict that
      //     a Request is coming that turns out to actually be a Simple-Response
      //
      // to fix this, we'd need to attempt to read the Request-Line and HTTP-Headers
      // If that failed, we'd need to backtrack somehow and parse a Simple-Response instead
      //
      // maybe we could throw a string containing the consumed portion of the input
      // to reconstruct the consumed input, we could serialize the partially-successful parse

      request req;
      lex >> req;
      self.body = req;
    }
    else
    {
      simple_response sr;
      lex >> sr;
      self.body = sr;
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

