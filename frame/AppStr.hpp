/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_APPSTR_HPP
#define FCGI_APPSTR_HPP

namespace fcgi_frame {

// See http://en.wikipedia.org/wiki/List_of_ISO_639-1_codes for correct codes.
enum APPSTR_LC
{
    APPSTR_NONE,
    APPSTR_EN,
    APPSTR_FI,
    APPSTR_SV,
    APPSTR_MAX_LC
};

//! Application strings class.
/*! Class is used to provide I18N feature to programs. Loads strings from the disk to memory and
  allows them to be referenced with a number.*/

class AppStr
{
  public:
    //! Constructor loads strings from given file.
    AppStr(const char* filepath, const char* locname);
    //! Free the memory accociated with this language.
    ~AppStr();

    //! Return std::string for a string number
    std::string getstd(uint16_t ndx) { return groups->getStdStr(ndx); }

    //! Return char ptr for a string number
    const char* getsp(uint16_t ndx) { return groups->getCPtr(ndx); }

    //! returns country code for this language.
    APPSTR_LC getLCode() { return lcode; }

    //! returns the ISO-639-1 name of the current language.
    const char* getLName();

    //! returns the total number of strings in this language.
    size_t getStringCount() { return groups->getTotalCount(); }

    //! returns the number of groups in this language
    size_t getGroupCount() const { return count_grp; }

    //! finds an enum number for a ISO-639-1 language code.
    static APPSTR_LC lcname2code(const char*);
    //! finds ISO-639-1 language code for menacon lib language number.
    static const char* code2lcname(APPSTR_LC);

  protected:
    //!\if INTCLASSES
    // ..................................................
    class ASArray
    {
      public:
        explicit ASArray(size_t count);
        ~ASArray();
        void add(size_t, const char*, size_t line);
        const char* get(uint16_t ndx);

        size_t size() const { return count; }

      protected:
        char** strarr;
        size_t count;
    };

    // ..................................................
    class ASGroup
    {
      public:
        explicit ASGroup(size_t count);
        ~ASGroup();
        void add(size_t, ASArray*, size_t line);
        const char* getCPtr(uint16_t ndx);

        std::string getStdStr(uint16_t ndx) { return std::string(getCPtr(ndx)); }

        size_t getTotalCount();

      protected:
        ASArray** groups;
        size_t size;
    };
    //!\endif

    size_t count_grp;
    // char lang[3];
    APPSTR_LC lcode;
    ASGroup* groups;

    static const char* lc_names[FRAME_LOCALES];
};
} // namespace fcgi_frame
#endif