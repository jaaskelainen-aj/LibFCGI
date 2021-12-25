/* This file is part of Fast CGI C++ library (libfcgi)
 * https://github.com/jaaskelainen-aj/libfcgi/wiki
 * 
 * Copyright (c) 2021: Antti Jääskeläinen
 * License: http://www.gnu.org/licenses/lgpl-2.1.html
 */
#ifndef LIBFCGI_HPP
#define LIBFCGI_HPP

#include "fcgisettings.h"

#include "driver/fcgidriver.hpp"
#include "driver/RingBuffer.hpp"
#include "driver/ParamData.hpp"
#include "driver/Request.hpp"
#include "driver/Driver.hpp"
#include "driver/Scheduler.hpp"

#include "frame/Framework.hpp"
#include "frame/AppStr.hpp"
#include "frame/base64.h"
#include "frame/MultiStr.hpp"
#include "frame/HtmlBuffer.hpp"
#include "frame/Includer.hpp"
#include "frame/SessionBase.hpp"
#include "frame/Serializer.hpp"
#include "frame/ErrorList.hpp"
#ifdef FCGI_SQL
  #include "frame/SqlInterface.hpp"
#endif
#ifdef FCGI_SMTP
  #include "frame/SmtpMailer.hpp"
#endif
#include "frame/PostData.hpp"
#include "frame/TmDatetime.hpp"

#endif