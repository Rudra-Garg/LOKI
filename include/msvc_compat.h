#pragma once

#if defined(_MSC_VER)
//  MSVC doesn't understand the __attribute__ keyword, so we define it as an empty macro
//  to effectively remove it from the code before the compiler sees it.
#define __attribute__(x)
#endif
