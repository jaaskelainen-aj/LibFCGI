# libfcgi
C++ Fast CGI protocol library and web framework for developing server side web programs.

## Introduction

LibFCGI framework has been inspired by a study written by Peter Simons and Ralph Babel in 2002: "FastCGI - The Forgotten Treasure". See https://www.nongnu.org/fastcgi/. I found the article as I was looking for web framework developed with C++ few years after the article was written. I was essentially looking for execution speed and small memory footprint. I had developed few services with Java and PHP but found them too slow and resource hungry, especially with complex applications and large number of users. My search for the ready library turned out empty but I was impressed about the idea presented in the artice. And I appreciated proof of concept source code that was available at the time.

I took the samples and created a driver library for Fast CGI. I quickly noticed that I need additional modules on top of this library. Session support, translations, buffering to name a few. I wanted to keep code based on Simon's and Babel's work separate so I kept the application framework in an another library. Design goal was to keep reusable parts of application in framework and driver, leaving only application specific implementation to be written for a web service. I've used the framework and driver in a few production level applications. Over the years I've been adding small things here and there to suit the needs of the applications I was developing at the time. These applications outlived their lifespan, but the framework and driver have much life left in them at least in my opinion. As I was starting my 7th iteration of framework, I thought that I would be good to do a bit of clean up and share the work with others.

## Dependensies

- Cpp4Scripts: Mostly process and path management library for C++ similar to Boost.
- DirectDB: Relational database library for C++.
- Hoedown: Markdown library written in C

# Tutorial
(TBD)
