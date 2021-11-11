/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 *
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#include <stdint.h>
#include <cstdlib>
#include <cpp4scripts.hpp>

using namespace c4s;

#include "driver/fcgidriver.cpp"

program_arguments args;

// -------------------------------------------------------------------------------------------------
int
do_hash(uint64_t salt)
{
    string hstr = args.get_value("-hash");
    cout << "Hash value for " << hstr << " = 0x" << hex
         << fcgi_driver::fnv_64bit_hash(hstr.c_str(), hstr.size(), salt) << "UL\n";
    return 0;
}

// -------------------------------------------------------------------------------------------------
int
main(int argc, char** argv)
{
    int rv = 0;
    uint64_t salt = 0;
    path_stack ps;
    cout << "LibFCGI build program\n";

    args += argument("-deb", false, "Sets the debug mode.");
    args += argument("-rel", false, "Sets the release mode.");
    args += argument("--salt", true, "Use given 64bit hex as salt for fnv_64bit_hash");
    args += argument("-export", true, "Export project files [ccdb|cmake]");
    args += argument("-ut", false, "Enables unit test build.");
    args += argument("-V", false, "Enable verbose build");
    args += argument("-clean", false, "Clean build directories and files.");
    args += argument("-hash", true, "Calculate hash value for the given string.");
    try {
        args.initialize(argc, argv, 1);
        ps.push(args.exe);
    } catch (const c4s_exception& ce) {
        cerr << "Error: " << ce.what() << endl;
        args.usage();
        return 1;
    }
    if (args.is_set("-hash")) {
        if (args.is_set("--salt"))
            salt = strtoull(args.get_value("--salt").c_str(), 0, 16);
        return do_hash(salt);
    }
    if (args.is_set("-clean")) {
        path dd("./debug/");
        path rd("./release/");
        if (dd.dirname_exists())
            dd.rmdir(true);
        if (rd.dirname_exists())
            rd.rmdir(true);
        cout << " Libary cleaned OK\n";
        return 0;
    }
    // construct builder and build
    try {
        builder* make = new builder_gcc("fcgi", &cout);
        make->set(BUILD::LIB);
        make->add(args.is_set("-deb") ? BUILD::DEB : BUILD::REL);
        if (args.is_set("-V"))
            make->add(BUILD::VERBOSE);
        if (args.is_set("-export"))
            make->add(BUILD::EXPORT);
        make->add_comp(
            "-fno-rtti -fuse-cxa-atexit -Wall -Wundef -Wno-ctor-dtor-privacy -Wnon-virtual-dtor "
            "-I/usr/local/include/cpp4scripts "
            "-I/usr/local/include/directdb");
        if (args.is_set("-ut"))
            make->add_comp("-DUNIT_TEST -DC4S_LOG_LEVEL=1");
        else if (args.is_set("-deb"))
            make->add_comp("-DC4S_LOG_LEVEL=2");
        else
            make->add_comp("-DC4S_LOG_LEVEL=3");
        rv = make->build();
        if (!rv && args.is_set("-export")) {
            path host("/Volumes/menacon/vshare/base7/libfcgi/");
            make->export_prj(args.get_value("-export"), args.exe, host);
        }
    } catch (const c4s_exception& ce) {
        cout << "build failed:" << ce.what() << '\n';
    }
    return rv;
}