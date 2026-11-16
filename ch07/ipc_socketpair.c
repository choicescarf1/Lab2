#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// 문자열 전체를 전송하는 함수
static int send_all(int fd, const char *text) {

    size_t length = strlen(text);

    while (length > 0) {

        ssize_t n = write(fd, text, length);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return -1;

        length -= (size_t)n;

        text += n;
    }

    return 0;
}

// 한 줄의 문자열을 수신하는 함수
static int get_line(
    int fd,
    char *buffer,
    size_t capacity
) {
    size_t used = 0;

    while (used + 1 < capacity) {

        char c;

        ssize_t n = read(fd, &c, 1);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return -1;

        if (c == '\n')
            break;

        buffer[used++] = c;
    }

    buffer[used] = '\0';

    return 0;
}

// 메인 함수
int main(void) {

    int endpoints[2];

    // UNIX 도메인 소켓 쌍 생성
    if (socketpair(
            AF_UNIX,
            SOCK_STREAM,
            0,
            endpoints
        ) < 0) {

        perror("socketpair");
        return 1;
    }

    fflush(stdout);

    // 자식 프로세스 생성
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    // 자식 프로세스
    if (pid == 0) {

        close(endpoints[0]);

        char buffer[256];

        // 부모로부터 메시지 수신
        if (get_line(
                endpoints[1],
                buffer,
                sizeof(buffer)
            ) < 0) {

            _exit(1);
        }

        printf(
            "[자식] 받은 메시지: %s\n",
            buffer
        );

        fflush(stdout);

        // 부모에게 응답 전송
        if (send_all(
                endpoints[1],
                "자식이 메시지를 잘 받았습니다.\n"
            ) < 0) {

            _exit(1);
        }

        close(endpoints[1]);

        _exit(0);
    }

    // 부모 프로세스
    close(endpoints[1]);

    int status = 0;

    // 자식에게 메시지 전송
    if (send_all(
            endpoints[0],
            "부모가 보내는 IPC 테스트 메시지\n"
        ) < 0) {

        perror("write");
        status = 1;
    }

    char response[256];

    // 자식으로부터 응답 수신
    if (!status &&
        get_line(
            endpoints[0],
            response,
            sizeof(response)
        ) == 0) {

        printf(
            "[부모] 받은 응답: %s\n",
            response
        );

    } else {
        status = 1;
    }

    close(endpoints[0]);

    int child_status;

    // 자식 프로세스 종료 확인
    if (waitpid(pid, &child_status, 0) < 0 ||
        !WIFEXITED(child_status) ||
        WEXITSTATUS(child_status) != 0) {

        status = 1;
    }

    return status;
}
