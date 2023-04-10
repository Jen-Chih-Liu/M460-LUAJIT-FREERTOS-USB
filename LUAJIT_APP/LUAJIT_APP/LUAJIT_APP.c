#define _CRT_SECURE_NO_WARNINGS  
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "windows.h"
#define DRIVER_NAME    "MyPortIO.exe"

int main(void)
{
#if 0
    UCHAR  driverLocation[MAX_PATH];
    DWORD driverLocLen = 0;
    driverLocLen = GetCurrentDirectoryA(MAX_PATH, driverLocation);
    if (!driverLocLen)
    {
        printf("GetCurrentDirectory failed! Error = %d \n", GetLastError());
        return FALSE;
    }
    strcat(driverLocation, "\\");
    strcat(driverLocation, DRIVER_NAME);
    printf("%s\n\r", driverLocation);
#endif
    lua_State *L = luaL_newstate();//create lua state
    luaL_openlibs(L);//open lua lib
    luaL_loadfile(L, "script.lua");
    lua_pcall(L, 0, 0, 0);//execution script
    lua_close(L);//close lua state

    return 0;
}
