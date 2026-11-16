#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

int main(int argc, char *argv[]) {

    // 1. 명령행 인자 확인
    if (argc != 3) {
        fprintf(stderr, "사용법: %s <원본파일> <대상파일>\n", argv[0]);
        return 1;
    }

    int src_fd = -1;
    int dst_fd = -1;
    int shm_fd = -1;

    void *src_map = MAP_FAILED;
    void *shared_map = MAP_FAILED;
    void *dst_map = MAP_FAILED;

    size_t size = 0;
    int result = 1;

    // 2. 원본 파일 열기
    src_fd = open(argv[1], O_RDONLY);

    if (src_fd < 0) {
        perror("원본 파일 열기 실패");
        goto cleanup;
    }

    // 3. 원본 파일 정보 확인
    struct stat info;

    if (fstat(src_fd, &info) < 0) {
        perror("파일 정보 확인 실패");
        goto cleanup;
    }

    if (!S_ISREG(info.st_mode) ||
        info.st_size < 0 ||
        (uintmax_t)info.st_size > SIZE_MAX) {

        fprintf(stderr, "올바른 일반 파일이 아닙니다.\n");
        goto cleanup;
    }

    size = (size_t)info.st_size;

    // 4. 대상 파일 생성
    dst_fd = open(argv[2], O_RDWR | O_CREAT | O_EXCL, 0644);

    if (dst_fd < 0) {
        perror("대상 파일 생성 실패");
        goto cleanup;
    }

    // 빈 파일인 경우 별도의 메모리 매핑 없이 처리
    if (size == 0) {
        printf("빈 파일 복사 완료\n");
        result = 0;
        goto cleanup;
    }

    // 대상 파일의 크기를 원본 파일과 동일하게 설정
    if (ftruncate(dst_fd, info.st_size) < 0) {
        perror("대상 파일 크기 설정 실패");
        goto cleanup;
    }

    // 5. POSIX 공유 메모리 생성
    char shm_name[64];

    snprintf(
        shm_name,
        sizeof(shm_name),
        "/lab2_copy_%ld",
        (long)getpid()
    );

    shm_fd = shm_open(
        shm_name,
        O_RDWR | O_CREAT | O_EXCL,
        0600
    );

    if (shm_fd < 0) {
        perror("공유 메모리 생성 실패");
        goto cleanup;
    }

    // 공유 메모리 이름 제거
    if (shm_unlink(shm_name) < 0) {
        perror("공유 메모리 이름 제거 실패");
        goto cleanup;
    }

    // 6. 공유 메모리 크기 설정
    if (ftruncate(shm_fd, info.st_size) < 0) {
        perror("공유 메모리 크기 설정 실패");
        goto cleanup;
    }

    // 7. 원본 파일 메모리 매핑
    src_map = mmap(
        NULL,
        size,
        PROT_READ,
        MAP_PRIVATE,
        src_fd,
        0
    );

    if (src_map == MAP_FAILED) {
        perror("원본 파일 mmap 실패");
        goto cleanup;
    }

    // 8. 공유 메모리 매핑
    shared_map = mmap(
        NULL,
        size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        shm_fd,
        0
    );

    if (shared_map == MAP_FAILED) {
        perror("공유 메모리 mmap 실패");
        goto cleanup;
    }

    // 9. 대상 파일 메모리 매핑
    dst_map = mmap(
        NULL,
        size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        dst_fd,
        0
    );

    if (dst_map == MAP_FAILED) {
        perror("대상 파일 mmap 실패");
        goto cleanup;
    }

    // 10. 원본 파일 -> 공유 메모리
    memcpy(shared_map, src_map, size);

    // 11. 공유 메모리 -> 대상 파일
    memcpy(dst_map, shared_map, size);

    // 12. 대상 파일의 변경 내용을 저장
    if (msync(dst_map, size, MS_SYNC) < 0) {
        perror("msync 실패");
        goto cleanup;
    }

    printf(
        "파일 복사 완료: %zu bytes\n",
        size
    );

    result = 0;

cleanup:

    // 13. 메모리 매핑 해제
    if (dst_map != MAP_FAILED)
        munmap(dst_map, size);

    if (shared_map != MAP_FAILED)
        munmap(shared_map, size);

    if (src_map != MAP_FAILED)
        munmap(src_map, size);

    // 14. 파일 및 공유 메모리 닫기
    if (shm_fd >= 0)
        close(shm_fd);

    if (src_fd >= 0)
        close(src_fd);

    if (dst_fd >= 0)
        close(dst_fd);

    // 오류가 발생한 경우 새로 생성한 불완전한 대상 파일 삭제
    if (result != 0 && dst_fd >= 0)
        unlink(argv[2]);

    return result;
}
