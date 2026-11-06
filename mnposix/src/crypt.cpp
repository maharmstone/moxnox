import ucrtbase;
import errno;

struct crypt_data;
extern "C" char* __crypt_r(const char* key, const char* salt, crypt_data* buf);

extern "C"
char* crypt(const char* key, const char* salt) {
    static char buf[128];

    auto ret = __crypt_r(key, salt, (struct crypt_data *)buf);

    // work around musl's dangerously non-compliant crypt implementation
    if (ret[0] == '*' || (ret[0] == 'x' && ret[1] == 0)) {
        *_errno() = EINVAL;
        return nullptr;
    }

    return ret;
}
