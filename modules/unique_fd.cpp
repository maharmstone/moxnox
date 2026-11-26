export module unique_fd;

import ucrtbase;

export class unique_fd {
public:
    unique_fd() : fd(-1) {
    }

    explicit unique_fd(int fd) : fd(fd) {
    }

    unique_fd(unique_fd&& that) noexcept {
        fd = that.fd;
        that.fd = -1;
    }

    unique_fd(const unique_fd&) = delete;
    unique_fd& operator=(const unique_fd&) = delete;

    unique_fd& operator=(unique_fd&& that) noexcept {
        if (fd >= 0)
            _close(fd);

        fd = that.fd;
        that.fd = -1;

        return *this;
    }

    ~unique_fd() {
        if (fd < 0)
            return;

        _close(fd);
    }

    explicit operator bool() const noexcept {
        return fd != -1;
    }

    void reset(int new_fd = -1) noexcept {
        if (fd >= 0)
            _close(fd);

        fd = new_fd;
    }

    int get() const noexcept {
        return fd;
    }

private:
    int fd;
};
