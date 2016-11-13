#ifndef LIBLBU_GLOBAL_H
#define LIBLBU_GLOBAL_H

#define LIBLBU_VERSION_MAJOR ${LIBLBU_VERSION_MAJOR}
#define LIBLBU_VERSION_MINOR ${LIBLBU_VERSION_MINOR}
#define LIBLBU_VERSION_PATCH ${LIBLBU_VERSION_PATCH}

#define LIBLBU_EXPORT __attribute__((visibility("default")))

namespace lbu {
    int LIBLBU_EXPORT version_major();
    int LIBLBU_EXPORT version_minor();
    int LIBLBU_EXPORT version_patch();
}

#endif // LIBLBU_GLOBAL_H
