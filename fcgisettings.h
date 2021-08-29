/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef FCGI_SETTINGS_H
#define FCGI_SETTINGS_H

// Max values for driver
#define DRIVER_PARAMKEYS 75
#define DRIVER_PARAMNAME 100
#define DRIVER_MPFIELD 50
#define DRIVER_POLL_FD 30
#define DRIVER_FNV64_SALT 0L

// Max values for framework
#define FRAME_LOCALES 4
#define FRAME_POSTKEYS 40
#define FRAME_POSTDATA 0x8000
#define FRAME_KEY_SIZE 8
#define FRAME_KEYS 128*FRAME_KEY_SIZE
#define FRAME_SID 24
#define FRAME_SMTP_MSG 20
#define FRAME_SMTP_FROM 64
#define FRAME_SMTP_HEADER 256

#endif