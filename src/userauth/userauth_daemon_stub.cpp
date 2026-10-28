/*
 * libuserauth_daemon.z.so replacement for HarmonyOS on the Redmi Note 10 Pro (sweet).
 *
 * The Huawei user_auth HDI service forwards every call to a Trusted
 * Application through this "CA" library (libteec -> iTrustee).  sweet has no
 * iTrustee, so the original library blocks forever trying to reach teecd and
 * the user_auth HDI is never published; the lock screen then cannot learn that
 * no credential exists and asks for a PIN that was never set.
 *
 * This replacement answers like an authentication framework with no enrolled
 * credentials: every query says "not enrolled", executors can register, and
 * enrollment/authentication are refused.  Result: swipe-to-unlock.
 */
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <dlfcn.h>

namespace OHOS {
template <typename T> class sptr;
namespace HDI {
namespace UserAuth {
namespace V3_0 {
struct ExecutorRegisterInfo;
struct EnrolledState;
struct CredentialInfo;
struct EnrolledInfo;
struct ExtUserInfo;
struct ScheduleInfo;
struct EnrollParam;
struct EnrollResultInfo;
struct AuthParam;
struct AuthResultInfo;
struct IdentifyResultInfo;
struct ReuseUnlockParam;
struct ReuseUnlockInfo;
struct GlobalConfigParam;
class IMessageCallback;
} // namespace V3_0
} // namespace UserAuth
} // namespace HDI
} // namespace OHOS

namespace {
constexpr int32_t RESULT_SUCCESS = 0;
constexpr int32_t RESULT_GENERAL_ERROR = 2;
constexpr int32_t RESULT_TYPE_NOT_SUPPORT = 5;
constexpr int32_t RESULT_NOT_ENROLLED = 10;

using LogFn = int (*)(int, int, unsigned int, const char *, const char *, ...);
void Log(const char *msg)
{
    static LogFn fn = reinterpret_cast<LogFn>(-1);
    if (fn == reinterpret_cast<LogFn>(-1)) {
        void *h = dlopen("libhilog.so", RTLD_NOW);
        fn = h ? reinterpret_cast<LogFn>(dlsym(h, "HiLogPrint")) : nullptr;
    }
    if (fn) {
        fn(3 /* LOG_CORE */, 4 /* INFO */, 0xD002400, "SWEET_UA", "%{public}s", msg);
    }
}
uint64_t g_nextExecutor = 1;
} // namespace

namespace OHOS {
namespace UserIam {
namespace Common {
int32_t Init()
{
    Log("user_auth CA stub: no TEE on sweet, no credentials");
    return 0;
}
int32_t Close()
{
    return 0;
}
} // namespace Common
} // namespace UserIam

namespace HDI {
namespace UserAuth {
using namespace V3_0;

class XCollieHelper {
public:
    XCollieHelper(const std::string &name, unsigned int timeout);
    ~XCollieHelper();
};
XCollieHelper::XCollieHelper(const std::string &, unsigned int) {}
XCollieHelper::~XCollieHelper() {}

int32_t SetLocalUdid(const std::string &)
{
    return RESULT_SUCCESS;
}

int32_t CaAddExecutor(const ExecutorRegisterInfo &, uint64_t &index, std::vector<uint8_t> &publicKey,
    std::vector<uint64_t> &templateIds)
{
    index = g_nextExecutor++;
    publicKey.assign(32, 0);
    templateIds.clear();
    return RESULT_SUCCESS;
}
int32_t CaDeleteExecutor(uint64_t)
{
    return RESULT_SUCCESS;
}
int32_t CaOpenSession(int32_t, std::vector<uint8_t> &challenge)
{
    challenge.resize(32);
    for (auto &b : challenge) {
        b = static_cast<uint8_t>(rand());
    }
    return RESULT_SUCCESS;
}
int32_t CaCloseSession(int32_t)
{
    return RESULT_SUCCESS;
}
int32_t CaGetAvailableStatus(int32_t, int32_t, uint32_t, int32_t &checkResult)
{
    checkResult = RESULT_NOT_ENROLLED;
    return RESULT_SUCCESS;
}
int32_t CaGetValidSolution(int32_t, const std::vector<int32_t> &, uint32_t, std::vector<int32_t> &validTypes)
{
    validTypes.clear();
    return RESULT_NOT_ENROLLED;
}
int32_t CaGetCredential(int32_t, int32_t, std::vector<CredentialInfo> &)
{
    return RESULT_SUCCESS; /* empty list: nothing enrolled */
}
int32_t CaGetCredentialById(uint64_t, CredentialInfo &)
{
    return RESULT_NOT_ENROLLED;
}
int32_t CaGetUserInfo(int32_t, uint64_t &secureUid, int32_t &pinSubType, std::vector<EnrolledInfo> &)
{
    secureUid = 0;
    pinSubType = 0;
    return RESULT_SUCCESS;
}
int32_t CaGetAllExtUserInfo(std::vector<ExtUserInfo> &)
{
    return RESULT_SUCCESS;
}
int32_t CaGetEnrolledState(int32_t, int32_t, EnrolledState &)
{
    return RESULT_NOT_ENROLLED;
}
int32_t CaBeginEnrollment(const std::vector<uint8_t> &, const EnrollParam &, ScheduleInfo &)
{
    return RESULT_GENERAL_ERROR;
}
int32_t CaUpdateEnrollmentResult(int32_t, const std::vector<uint8_t> &, EnrollResultInfo &)
{
    return RESULT_GENERAL_ERROR;
}
int32_t CaCancelEnrollment(int32_t)
{
    return RESULT_SUCCESS;
}
int32_t CaDeleteCredential(int32_t, uint64_t, const std::vector<uint8_t> &, CredentialInfo &)
{
    return RESULT_NOT_ENROLLED;
}
int32_t CaDeleteUser(int32_t, const std::vector<uint8_t> &, std::vector<CredentialInfo> &,
    std::vector<uint8_t> &)
{
    return RESULT_SUCCESS;
}
int32_t CaEnforceDeleteUser(int32_t, std::vector<CredentialInfo> &)
{
    return RESULT_SUCCESS;
}
int32_t CaBeginAuthentication(uint64_t, bool, const AuthParam &, std::vector<ScheduleInfo> &)
{
    return RESULT_NOT_ENROLLED;
}
int32_t CaUpdateAuthenticationResult(uint64_t, const std::vector<uint8_t> &, AuthResultInfo &, EnrolledState &)
{
    return RESULT_GENERAL_ERROR;
}
int32_t CaCancelAuthentication(uint64_t)
{
    return RESULT_SUCCESS;
}
int32_t CaBeginIdentification(uint64_t, int32_t, const std::vector<uint8_t> &, uint32_t, ScheduleInfo &)
{
    return RESULT_TYPE_NOT_SUPPORT;
}
int32_t CaUpdateIdentificationResult(uint64_t, const std::vector<uint8_t> &, IdentifyResultInfo &)
{
    return RESULT_GENERAL_ERROR;
}
int32_t CaCancelIdentification(uint64_t)
{
    return RESULT_SUCCESS;
}
int32_t CaCheckReuseUnlockResult(const ReuseUnlockParam &, ReuseUnlockInfo &)
{
    return RESULT_NOT_ENROLLED;
}
int32_t CaSetGlobalConfigParam(const GlobalConfigParam &)
{
    return RESULT_SUCCESS;
}
int32_t CaRegisterMessageCallback(const sptr<IMessageCallback> &)
{
    return RESULT_SUCCESS;
}
int32_t CaPrepareRemoteAuth(const std::string &)
{
    return RESULT_TYPE_NOT_SUPPORT;
}
int32_t CaGetLocalScheduleFromMessage(const std::string &, const std::vector<uint8_t> &, ScheduleInfo &)
{
    return RESULT_TYPE_NOT_SUPPORT;
}
int32_t CaGetAuthResultFromMessage(const std::string &, const std::vector<uint8_t> &, AuthResultInfo &)
{
    return RESULT_TYPE_NOT_SUPPORT;
}
int32_t CaGetSignedExecutorInfo(const std::vector<int32_t> &, int32_t, const std::string &,
    std::vector<uint8_t> &)
{
    return RESULT_TYPE_NOT_SUPPORT;
}
} // namespace UserAuth
} // namespace HDI
} // namespace OHOS
