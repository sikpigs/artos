#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

// No file descriptors or libc console backend yet. Report failure rather than
// pretending that input/output succeeded. Assertion failures still call abort.
extern "C"
{
    int _close(int)
    {
        errno = ENOSYS;
        return -1;
    }

    int _fstat(int, struct stat*)
    {
        errno = ENOSYS;
        return -1;
    }

    int _isatty(int)
    {
        errno = ENOSYS;
        return 0;
    }

    _off_t _lseek(int, _off_t, int)
    {
        errno = ENOSYS;
        return -1;
    }

    _READ_WRITE_RETURN_TYPE _read(int, void*, size_t)
    {
        errno = ENOSYS;
        return -1;
    }

    _READ_WRITE_RETURN_TYPE _write(int, const void*, size_t)
    {
        errno = ENOSYS;
        return -1;
    }
}
