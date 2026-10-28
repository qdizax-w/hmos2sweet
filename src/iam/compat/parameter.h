/* minimal begetutil parameter API (libbegetutil.z.so provides it at runtime) */
#ifndef SWEET_COMPAT_PARAMETER_H
#define SWEET_COMPAT_PARAMETER_H
#ifdef __cplusplus
extern "C" {
#endif
int GetParameter(const char *key, const char *def, char *value, unsigned int len);
int SetParameter(const char *key, const char *value);
int GetDevUdid(char *udid, int size);
#ifdef __cplusplus
}
#endif
#endif
