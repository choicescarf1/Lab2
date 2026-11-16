#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// 디렉토리 경로와 파일 이름을 연결하는 함수
static char *join_path(
    const char *directory,
    const char *name
) {
    size_t size =
        strlen(directory) + 1 +
        strlen(name) + 1;

    char *path = malloc(size);

    if (path)
        snprintf(
            path,
            size,
            "%s/%s",
            directory,
            name
        );

    return path;
}

// 파일 이름을 정렬하기 위한 비교 함수
static int cmp_names(
    const void *left,
    const void *right
) {
    return strcmp(
        *(const char *const *)left,
        *(const char *const *)right
    );
}

// 재귀적으로 디렉토리를 탐색하는 함수
static int show_recursive(
    const char *path,
    int *printed_section
) {
    DIR *dir = opendir(path);

    if (!dir) {
        perror(path);
        return -1;
    }

    size_t count = 0;
    size_t capacity = 0;

    char **names = NULL;

    int status = 0;

    struct dirent *entry;

    errno = 0;

    // 현재 디렉토리의 모든 항목 읽기
    while ((entry = readdir(dir)) != NULL) {

        // 숨김 파일 제외
        if (entry->d_name[0] == '.')
            continue;

        // 저장 공간이 부족하면 크기 확장
        if (count == capacity) {

            size_t new_capacity =
                capacity == 0 ? 16 : capacity * 2;

            char **expanded = realloc(
                names,
                new_capacity * sizeof(*names)
            );

            if (!expanded) {
                perror("realloc");
                status = -1;
                break;
            }

            names = expanded;
            capacity = new_capacity;
        }

        // 파일 이름 저장
        names[count] = strdup(entry->d_name);

        if (!names[count]) {
            perror("strdup");
            status = -1;
            break;
        }

        ++count;
        errno = 0;
    }

    if (errno != 0 && status == 0) {
        perror("readdir");
        status = -1;
    }

    if (closedir(dir) < 0) {
        perror("closedir");
        status = -1;
    }

    if (status != 0)
        goto cleanup;

    // 파일 이름 정렬
    qsort(
        names,
        count,
        sizeof(*names),
        cmp_names
    );

    if (*printed_section)
        putchar('\n');

    printf("%s:\n", path);

    *printed_section = 1;

    // 현재 디렉토리의 파일 목록 출력
    for (size_t i = 0; i < count; ++i)
        puts(names[i]);

    // 하위 디렉토리 재귀 탐색
    for (size_t i = 0; i < count; ++i) {

        char *child = join_path(
            path,
            names[i]
        );

        if (!child) {
            perror("malloc");
            status = -1;
            break;
        }

        struct stat info;

        if (lstat(child, &info) < 0) {
            perror(child);
            status = -1;
        }

        else if (S_ISDIR(info.st_mode) &&
                 show_recursive(
                     child,
                     printed_section
                 ) < 0) {
            status = -1;
        }

        free(child);
    }

// 동적 메모리 해제
cleanup:

    for (size_t i = 0; i < count; ++i)
        free(names[i]);

    free(names);

    return status;
}

// 메인 함수
int main(int argc, char **argv) {

    if (argc > 2) {
        fprintf(
            stderr,
            "사용법: %s [디렉토리]\n",
            argv[0]
        );

        return 1;
    }

    const char *path =
        argc == 2 ? argv[1] : ".";

    struct stat info;

    if (lstat(path, &info) < 0) {
        perror(path);
        return 1;
    }

    // 일반 파일이면 이름만 출력
    if (!S_ISDIR(info.st_mode)) {
        puts(path);
        return 0;
    }

    int printed_section = 0;

    return show_recursive(
        path,
        &printed_section
    ) == 0 ? 0 : 1;
}
