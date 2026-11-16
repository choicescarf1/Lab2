#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <mqueue.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MESSAGE_SIZE 256

// 메시지 수신 스레드에서 사용할 구조체
struct receiver_args {
    mqd_t input;
    atomic_bool *running;
};

// 메시지 큐 이름 생성
static void queue_names(
    const char *room,
    char *a_to_b,
    char *b_to_a,
    size_t size
) {
    snprintf(
        a_to_b,
        size,
        "/lab2_%ld_%s_a2b",
        (long)getuid(),
        room
    );

    snprintf(
        b_to_a,
        size,
        "/lab2_%ld_%s_b2a",
        (long)getuid(),
        room
    );
}

// 채팅방 이름 검사
static int valid_room(const char *room) {

    size_t len = strlen(room);

    if (len == 0 || len > 24)
        return 0;

    for (size_t i = 0; i < len; ++i) {

        unsigned char c = (unsigned char)room[i];

        if (!isalnum(c) &&
            c != '_' &&
            c != '-') {

            return 0;
        }
    }

    return 1;
}

// 메시지를 수신하는 스레드 함수
static void *receive_messages(void *data) {

    struct receiver_args *args = data;

    char message[MESSAGE_SIZE + 1];

    while (atomic_load(args->running)) {

        struct timespec timeout;

        if (clock_gettime(
                CLOCK_REALTIME,
                &timeout
            ) < 0) {

            break;
        }

        timeout.tv_sec += 1;

        // 메시지 큐에서 데이터 수신
        ssize_t len = mq_timedreceive(
            args->input,
            message,
            MESSAGE_SIZE,
            NULL,
            &timeout
        );

        if (len < 0) {

            if (errno == EINTR ||
                errno == ETIMEDOUT) {

                continue;
            }

            perror("mq_timedreceive");

            atomic_store(args->running, 0);
            break;
        }

        message[len] = '\0';

        // 상대방 종료 확인
        if (strcmp(message, "/quit") == 0) {

            printf(
                "\n[상대방이 채팅을 종료했습니다.]\n"
            );

            atomic_store(args->running, 0);

        } else {

            printf(
                "\n[상대방] %s\n",
                message
            );
        }

        fflush(stdout);
    }

    return NULL;
}

// 메인 함수
int main(int argc, char **argv) {

    // 명령행 인자 검사
    if (argc != 3 ||
        !valid_room(argv[2]) ||
        (strcmp(argv[1], "alice") != 0 &&
         strcmp(argv[1], "bob") != 0 &&
         strcmp(argv[1], "--cleanup") != 0)) {

        fprintf(
            stderr,
            "사용법: %s alice|bob|--cleanup <방이름>\n",
            argv[0]
        );

        return 1;
    }

    char a_to_b[64];
    char b_to_a[64];

    // 메시지 큐 이름 생성
    queue_names(
        argv[2],
        a_to_b,
        b_to_a,
        sizeof(a_to_b)
    );

    // 메시지 큐 삭제
    if (strcmp(argv[1], "--cleanup") == 0) {

        if (mq_unlink(a_to_b) < 0 &&
            errno != ENOENT) {

            perror("mq_unlink a2b");
            return 1;
        }

        if (mq_unlink(b_to_a) < 0 &&
            errno != ENOENT) {

            perror("mq_unlink b2a");
            return 1;
        }

        printf(
            "'%s' 방의 POSIX 메시지 큐를 정리했습니다.\n",
            argv[2]
        );

        return 0;
    }

    // 메시지 큐 속성 설정
    struct mq_attr attributes = {0};

    attributes.mq_maxmsg = 10;
    attributes.mq_msgsize = MESSAGE_SIZE;

    // 첫 번째 메시지 큐 생성
    mqd_t a = mq_open(
        a_to_b,
        O_CREAT | O_RDWR,
        0600,
        &attributes
    );

    if (a == (mqd_t)-1) {
        perror("mq_open a2b");
        return 1;
    }

    // 두 번째 메시지 큐 생성
    mqd_t b = mq_open(
        b_to_a,
        O_CREAT | O_RDWR,
        0600,
        &attributes
    );

    if (b == (mqd_t)-1) {

        perror("mq_open b2a");

        mq_close(a);

        return 1;
    }

    // Alice와 Bob의 송수신 큐 구분
    int alice = strcmp(argv[1], "alice") == 0;

    mqd_t outgoing = alice ? a : b;
    mqd_t incoming = alice ? b : a;

    // 채팅 실행 여부
    atomic_bool running = ATOMIC_VAR_INIT(1);

    struct receiver_args args = {
        .input = incoming,
        .running = &running
    };

    pthread_t receiver;

    // 수신 스레드 생성
    if (pthread_create(
            &receiver,
            NULL,
            receive_messages,
            &args
        ) != 0) {

        fprintf(
            stderr,
            "수신 스레드 생성 실패\n"
        );

        mq_close(a);
        mq_close(b);

        return 1;
    }

    printf(
        "방=%s 역할=%s. 다른 터미널에서 반대 역할을 실행하세요.\n",
        argv[2],
        argv[1]
    );

    puts(
        "메시지 입력 후 Enter, 종료는 /quit"
    );

    char line[MESSAGE_SIZE + 1];

    int status = 0;

    // 사용자 메시지 입력 반복
    while (atomic_load(&running)) {

        struct pollfd stdin_poll = {
            .fd = STDIN_FILENO,
            .events = POLLIN
        };

        int ready = poll(
            &stdin_poll,
            1,
            250
        );

        if (ready < 0) {

            if (errno == EINTR)
                continue;

            perror("poll");

            status = 1;
            break;
        }

        if (ready == 0)
            continue;

        if ((stdin_poll.revents &
            (POLLIN | POLLHUP)) == 0) {

            break;
        }

        // 사용자 입력
        if (!fgets(
                line,
                sizeof(line),
                stdin
            )) {

            break;
        }

        size_t len = strlen(line);

        if (len > 0 &&
            line[len - 1] == '\n') {

            line[--len] = '\0';

        } else if (!feof(stdin)) {

            int c;

            while ((c = getchar()) != '\n' &&
                   c != EOF) {}

            fputs(
                "메시지는 255바이트 이내여야 합니다.\n",
                stderr
            );

            continue;
        }

        struct timespec deadline;

        if (clock_gettime(
                CLOCK_REALTIME,
                &deadline
            ) < 0) {

            perror("clock_gettime");

            status = 1;
            break;
        }

        deadline.tv_sec += 2;

        // 상대방에게 메시지 전송
        if (mq_timedsend(
                outgoing,
                line,
                len + 1,
                0,
                &deadline
            ) < 0) {

            perror("mq_timedsend");

            status = 1;
            break;
        }

        if (strcmp(line, "/quit") == 0)
            break;
    }

    // 수신 스레드 종료
    atomic_store(&running, 0);

    pthread_join(receiver, NULL);

    // 메시지 큐 닫기
    mq_close(a);
    mq_close(b);

    puts(
        "채팅 종료. 두 프로세스 종료 후 --cleanup으로 큐를 삭제하세요."
    );

    return status;
}
