#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// 파일의 데이터를 복사하는 함수
static int copy_bytes(int src, int dst) {
    char buffer[8192];

    for (;;) {
        ssize_t n = read(src, buffer, sizeof(buffer));

        if (n == 0)
            return 0;

        if (n < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        for (ssize_t off = 0; off < n;) {
            ssize_t written = write(
                dst,
                buffer + off,
                (size_t)(n - off)
            );

            if (written < 0 && errno == EINTR)
                continue;

            if (written <= 0)
                return -1;

            off += written;
        }
    }
}

// 디렉토리의 내용을 출력하는 함수
static int list_directory(const char *path) {
    DIR *dir = opendir(path);

    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    printf("\n[%s 디렉토리 항목]\n", path);

    struct dirent *entry;

    errno = 0;

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        size_t len =
            strlen(path) + 1 +
            strlen(entry->d_name) + 1;

        char *fullpath = malloc(len);

        if (!fullpath) {
            perror("malloc");
            closedir(dir);
            return -1;
        }

        snprintf(
            fullpath,
            len,
            "%s/%s",
            path,
            entry->d_name
        );

        struct stat info;

        if (lstat(fullpath, &info) == 0) {

            const char *type =
                S_ISDIR(info.st_mode) ? "디렉토리" :
                S_ISREG(info.st_mode) ? "일반 파일" :
                S_ISLNK(info.st_mode) ? "심볼릭 링크" :
                "기타";

            printf(
                "%-24s %-14s %lld bytes\n",
                entry->d_name,
                type,
                (long long)info.st_size
            );

        } else {
            perror(fullpath);
        }

        free(fullpath);

        errno = 0;
    }

    if (errno != 0) {
        perror("readdir");
        closedir(dir);
        return -1;
    }

    if (closedir(dir) < 0) {
        perror("closedir");
        return -1;
    }

    return 0;
}

// 메인 함수
int main(int argc, char **argv) {

    if (argc != 4) {
        fprintf(
            stderr,
            "사용법: %s <원본파일> <새 복사본파일> <조회할 디렉토리>\n",
            argv[0]
        );

        return 1;
    }

    // 원본 파일 열기
    int src = open(argv[1], O_RDONLY);

    if (src < 0) {
        perror("open 원본");
        return 1;
    }

    // 파일 정보 확인
    struct stat info;

    if (fstat(src, &info) < 0 ||
        !S_ISREG(info.st_mode)) {

        fprintf(
            stderr,
            "원본은 읽을 수 있는 일반 파일이어야 합니다.\n"
        );

        close(src);
        return 1;
    }

    printf(
        "원본 크기(fstat): %lld bytes\n",
        (long long)info.st_size
    );

    // 새로운 파일 생성
    int dst = open(
        argv[2],
        O_WRONLY | O_CREAT | O_EXCL,
        0644
    );

    if (dst < 0) {
        perror("open 새 복사본");
        close(src);
        return 1;
    }

    int status = 0;

    // 파일 복사
    if (copy_bytes(src, dst) < 0) {
        perror("read/write");
        status = 1;
    }

    // 파일 닫기
    if (close(src) < 0) {
        perror("close 원본");
        status = 1;
    }

    if (close(dst) < 0) {
        perror("close 복사본");
        status = 1;
    }

    if (status != 0) {
        unlink(argv[2]);
        return 1;
    }

    printf(
        "새 파일 생성 완료: %s\n",
        argv[2]
    );

    // 디렉토리 정보 출력
    return list_directory(argv[3]) == 0 ? 0 : 1;
}
