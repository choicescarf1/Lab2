#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// 시그널 수신 여부를 저장하는 변수
static volatile sig_atomic_t got_signal = 0;

// SIGUSR1 시그널 처리 함수
static void on_sigusr1(int signum) {

    if (signum == SIGUSR1)
        got_signal = 1;
}

int main(void) {

    int ready[2];

    // 부모와 자식 사이의 통신용 파이프 생성
    if (pipe(ready) < 0) {
        perror("pipe");
        return 1;
    }

    sigset_t blocked, original;

    sigemptyset(&blocked);
    sigaddset(&blocked, SIGUSR1);

    // SIGUSR1 시그널 차단
    if (sigprocmask(
            SIG_BLOCK,
            &blocked,
            &original
        ) < 0) {

        perror("sigprocmask");
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

        close(ready[0]);

        struct sigaction action = {0};

        action.sa_handler = on_sigusr1;

        sigemptyset(&action.sa_mask);

        // 시그널 핸들러 등록
        if (sigaction(
                SIGUSR1,
                &action,
                NULL
            ) < 0) {

            perror("sigaction");
            _exit(1);
        }

        printf(
            "[자식 PID=%ld] SIGUSR1 수신 대기\n",
            (long)getpid()
        );

        fflush(stdout);

        // 부모에게 준비 완료 통지
        if (write(ready[1], "R", 1) != 1) {

            perror("write");
            _exit(1);
        }

        close(ready[1]);

        sigset_t wait_mask = original;

        sigdelset(&wait_mask, SIGUSR1);

        // SIGUSR1 수신 대기
        while (!got_signal)
            sigsuspend(&wait_mask);

        // 기존 시그널 마스크 복구
        sigprocmask(
            SIG_SETMASK,
            &original,
            NULL
        );

        puts(
            "[자식] 등록한 sigaction 핸들러로 SIGUSR1을 처리했습니다."
        );

        fflush(stdout);

        _exit(0);
    }

    // 부모 프로세스
    close(ready[1]);

    char signal_ready;
    ssize_t n;

    // 자식의 준비 완료 메시지 수신
    do {
        n = read(
            ready[0],
            &signal_ready,
            1
        );

    } while (n < 0 && errno == EINTR);

    close(ready[0]);

    if (n != 1) {

        fprintf(
            stderr,
            "자식 프로세스 준비 실패\n"
        );

        waitpid(pid, NULL, 0);

        return 1;
    }

    printf(
        "[부모 PID=%ld] 자식 %ld에 SIGUSR1을 전송합니다.\n",
        (long)getpid(),
        (long)pid
    );

    fflush(stdout);

    // 자식 프로세스에 시그널 전송
    if (kill(pid, SIGUSR1) < 0) {

        perror("kill");

        waitpid(pid, NULL, 0);

        return 1;
    }

    int status;

    // 자식 프로세스 종료 대기
    if (waitpid(pid, &status, 0) < 0) {

        perror("waitpid");
        return 1;
    }

    // 자식 프로세스 정상 종료 여부 확인
    if (!WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {

        fprintf(
            stderr,
            "자식이 비정상적으로 종료했습니다.\n"
        );

        return 1;
    }

    puts(
        "[부모] waitpid로 자식의 정상 종료를 확인했습니다."
    );

    sigprocmask(
        SIG_SETMASK,
        &original,
        NULL
    );

    return 0;
}
