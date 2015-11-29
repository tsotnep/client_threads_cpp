#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <SDKDDKVer.h>
#include <stdio.h>
#include <tchar.h>
#include "Winsock2.h" // necessary for sockets, Windows.h is not needed.
#include "mswsock.h"
#include "process.h"  // necessary for threading