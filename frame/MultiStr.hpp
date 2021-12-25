/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_MULTISTR_HPP
#define FCGI_MULTISTR_HPP

namespace fcgi_frame {

const int MAX_MS_TOKENS = 16;
const int MAX_MS_LEN    = 256;

//! Class that breaks a string into multiple smaller strings separated by tokens and whitespace.
class MultiStr
{
  public:
    //! Initializes empty object.
    MultiStr() { clean(); }
    //! Initializes class by breaking the original string into pieces.
    MultiStr(const char* orig, const char* token, const char* white) {
      if (!set(orig, token, white))
        clean();
    }
    // Empty destructor.
    ~MultiStr() { }
    //! Replaces earlier content with a new.
    bool set(const char* orig, const char* token, const char* white);
    //! Starts iteration and returns pointer to first sub string.
    const char* getFirst();
    //! Continues iteration and returns pointer to next substring.
    const char* getNext();
    //! Returns number of substrings found.
    size_t size() { return multi ? token_count : 0; }
    const char* operator[](size_t ndx) const;

  protected:
    void clean();

    size_t length;
    size_t token_count;
    char multi[MAX_MS_LEN];
    const char* offsets[MAX_MS_TOKENS];
    size_t ndx;
};

} // namespace fcgi_frame
#endif