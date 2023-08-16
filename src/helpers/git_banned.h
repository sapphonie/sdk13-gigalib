#ifndef BANNED_H
#define BANNED_H

/*
 * This header lists functions that have been banned from our code base,
 * because they're too easy to misuse (and even if used correctly,
 * complicate audits). Including this header turns them into compile-time
 * errors.
 */

#define BANNED(func) sorry_##func##_is_a_banned_function

#undef V_strcpy
#define V_strcpy(x,y) BANNED(V_strcpy)
#undef V_strcat
#define V_strcat(x,y) BANNED(V_strcat)
#undef V_strncpy
#define V_strncpy(x,y,n) BANNED(V_strncpy)
#undef V_strncat
#define V_strncat(x,y,n) BANNED(V_strncat)
#undef V_strtok
#define V_strtok(x,y) BANNED(V_strtok)

#undef strtok_r
#define strtok_r(x,y,z) BANNED(strtok_r)

#undef sprintf
#undef vsprintf
#define sprintf(...) BANNED(sprintf)
#define vsprintf(...) BANNED(vsprintf)

#undef gmtime
#define gmtime(t) BANNED(gmtime)
#undef localtime
#define localtime(t) BANNED(localtime)
#undef ctime
#define ctime(t) BANNED(ctime)
#undef ctime_r
#define ctime_r(t, buf) BANNED(ctime_r)
#undef asctime
#define asctime(t) BANNED(asctime)
#undef asctime_r
#define asctime_r(t, buf) BANNED(asctime_r)

// additional win funcs
#undef wcscpy
#define wcscpy(x,y) BANNED(wcscpy)



#endif /* BANNED_H */
