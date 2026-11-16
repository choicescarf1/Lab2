#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// 문자열 전체를 파이프에 전송하는 함수
static int send_all(int fd, const char *text) {

    size_t remain = strlen(text);

    while (remain > 0) {

        ssize_t n = write(fd, text, remain);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
            return -1;

        text += n;
        remain -= (size_t)n;
    }

    return 0;
}

// 파이프에서 문자열을 한 줄 읽는 함수
static int read_line(
    int fd,
    char *buffer,
    size_t size
) {
    size_t used = 0;

    for (;;) {

        char c;

        ssize_t n = read(fd, &c, 1);

        if (n < 0 && errno == EINTR)
            continue;

        if (n < 0)
            return -1;

        if (n == 0) {
            buffer[used] = '\0';
            return used > 0 ? 1 : 0;
        }

        if (c == '\n') {
            buffer[used] = '\0';
            return 1;
        }

        if (used + 1 >= size)
            return -1;

        buffer[used++] = c;
    }
}

// 메인 함수
int main(void) {

    int parent_to_child[2];
    int child_to_parent[2];

    // 부모 -> 자식 파이프 생성
    if (pipe(parent_to_child) < 0 ||
        pipe(child_to_parent) < 0) {

        perror("pipe");
        return 1;
    }

    // 연결이 끊어진 파이프에 쓰더라도
    // 프로세스가 즉시 종료되지 않도록 설정
    signal(SIGPIPE, SIG_IGN);

    fflush(stdout);

    // 자식 프로세스 생성
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    // 자식 프로세스
    if (pid == 0) {

        close(parent_to_child[1]);
        close(child_to_parent[0]);

        char text[512];
        char reply[700];

        // 부모의 메시지 수신
        while (read_line(
                parent_to_child[0],
                text,
                sizeof(text)
            ) == 1) {

            // 종료 명령 확인
            if (strcmp(text, "/quit") == 0) {

                send_all(
                    child_to_parent[1],
                    "상대 프로세스 종료\n"
                );

                break;
            }

            // 자동 응답 생성
            snprintf(
                reply,
                sizeof(reply),
                "프로세스 B> '%s' 메시지를 받았습니다.\n",
                text
            );

            // 부모에게 응답 전송
            if (send_all(
                    child_to_parent[1],
                    reply
                ) < 0) {

                break;
            }
        }

        close(parent_to_child[0]);
        close(child_to_parent[1]);

        _exit(0);
    }

    // 부모 프로세스
    close(parent_to_child[0]);
    close(child_to_parent[1]);

    puts(
        "익명 파이프 2개로 구현한 부모-자식 프로세스 채팅"
    );

    puts(
        "한 프로세스가 입력하면 다른 프로세스가 응답합니다. 종료: /quit"
    );

    char input[512];
    char reply[700];

    int status = 0;

    // 사용자 입력 반복
    for (;;) {

        printf("프로세스 A> ");

        fflush(stdout);

        // 사용자 메시지 입력
        if (!fgets(
                input,
                sizeof(input),
                stdin
            )) {

            strcpy(input, "/quit\n");
        }

        else if (strchr(input, '\n') == NULL) {

            int c;

            while ((c = getchar()) != '\n' &&
                   c != EOF) {}

            puts(
                "메시지는 510자 이내로 입력하세요."
            );

            continue;
        }

        // 자식 프로세스로 메시지 전송
        if (send_all(
                parent_to_child[1],
                input
            ) < 0) {

            perror("파이프 쓰기");
            status = 1;
            break;
        }

        // 자식 프로세스의 응답 수신
        if (read_line(
                child_to_parent[0],
                reply,
                sizeof(reply)
            ) != 1) {

            fprintf(
                stderr,
                "상대 프로세스와의 연결이 끊겼습니다.\n"
            );

            status = 1;
            break;
        }

        puts(reply);

        // 종료 명령 확인
        if (strcmp(input, "/quit\n") == 0)
            break;
    }

    // 파이프 닫기
    close(parent_to_child[1]);
    close(child_to_parent[0]);

    int child_status;

    // 자식 프로세스 종료 확인
    if (waitpid(pid, &child_status, 0) < 0 ||
        !WIFEXITED(child_status) ||
        WEXITSTATUS(child_status) != 0) {

        status = 1;
    }

    return status;
}
