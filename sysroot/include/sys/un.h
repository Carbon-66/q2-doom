#ifndef _SYS_UN_H
#define _SYS_UN_H
struct sockaddr_un { unsigned short sun_family; char sun_path[108]; };
#endif
