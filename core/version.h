#ifndef PD_VERSION_H
#define PD_VERSION_H
#define PD_VERSION "0.18.0"
#ifdef PD_PRIVATE_BUILD
#define PD_BUILD_LABEL "V" PD_VERSION "P"
#else
#define PD_BUILD_LABEL "V" PD_VERSION
#endif
#endif
