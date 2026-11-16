#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <systemd/sd-bus.h>

// D-Bus 서비스 정보
#define SERVICE "com.example.Lab2.Notify"
#define OBJECT_PATH "/com/example/Lab2/Notify"
#define INTERFACE "com.example.Lab2.Notify"

// 서버 실행 상태
static volatile sig_atomic_t keep_running = 1;

// 종료 시그널 처리 함수
static void stop_server(int signo) {
    (void)signo;

    keep_running = 0;
}

// 클라이언트가 보낸 메시지를 처리하는 함수
static int notify_handler(
    sd_bus_message *message,
    void *userdata,
    sd_bus_error *error
) {
    (void)userdata;
    (void)error;

    const char *text = NULL;

    // 메시지에서 문자열 읽기
    int r = sd_bus_message_read(message, "s", &text);

    if (r < 0) {
        return r;
    }

    // 수신한 알림 출력
    printf("[D-Bus 알림 수신] %s\n", text);
    fflush(stdout);

    // 다른 구독자에게 알림 시그널 전송
    sd_bus *bus = sd_bus_message_get_bus(message);

    r = sd_bus_emit_signal(
        bus,
        OBJECT_PATH,
        INTERFACE,
        "Notification",
        "s",
        text
    );

    if (r < 0) {
        return r;
    }

    // 클라이언트에게 응답
    return sd_bus_reply_method_return(
        message,
        "s",
        "서버가 알림을 수신했습니다."
    );
}

// D-Bus 메서드 및 시그널 등록
static const sd_bus_vtable notify_vtable[] = {

    SD_BUS_VTABLE_START(0),

    SD_BUS_METHOD(
        "Notify",
        "s",
        "s",
        notify_handler,
        SD_BUS_VTABLE_UNPRIVILEGED
    ),

    SD_BUS_SIGNAL(
        "Notification",
        "s",
        0
    ),

    SD_BUS_VTABLE_END
};

int main(void) {

    sd_bus *bus = NULL;
    sd_bus_slot *slot = NULL;

    // Ctrl+C 종료 처리
    struct sigaction action = {0};

    action.sa_handler = stop_server;

    sigemptyset(&action.sa_mask);

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    // 1. D-Bus 세션 버스에 연결
    int r = sd_bus_default_user(&bus);

    if (r < 0) {
        fprintf(
            stderr,
            "세션 버스 연결 실패: %s\n",
            strerror(-r)
        );

        return 1;
    }

    // 2. D-Bus 객체 및 메서드 등록
    r = sd_bus_add_object_vtable(
        bus,
        &slot,
        OBJECT_PATH,
        INTERFACE,
        notify_vtable,
        NULL
    );

    if (r < 0) {
        fprintf(
            stderr,
            "객체 등록 실패: %s\n",
            strerror(-r)
        );

        goto fail;
    }

    // 3. D-Bus 서비스 이름 등록
    r = sd_bus_request_name(
        bus,
        SERVICE,
        0
    );

    if (r < 0) {
        fprintf(
            stderr,
            "서비스 등록 실패: %s\n",
            strerror(-r)
        );

        goto fail;
    }

    printf("D-Bus 알림 서버 준비 완료\n");
    printf("서비스: %s\n", SERVICE);
    printf("종료: Ctrl+C\n");

    fflush(stdout);

    // 4. 메시지 수신 대기
    while (keep_running) {

        r = sd_bus_process(bus, NULL);

        if (r < 0) {

            if (r == -EINTR) {
                continue;
            }

            fprintf(
                stderr,
                "메시지 처리 실패: %s\n",
                strerror(-r)
            );

            goto fail;
        }

        if (r > 0) {
            continue;
        }

        // 메시지가 없으면 대기
        r = sd_bus_wait(bus, (uint64_t)-1);

        if (r < 0 && r != -EINTR) {

            fprintf(
                stderr,
                "버스 대기 실패: %s\n",
                strerror(-r)
            );

            goto fail;
        }
    }

    // 5. 자원 정리
    sd_bus_slot_unref(slot);
    sd_bus_unref(bus);

    return 0;

fail:
    sd_bus_slot_unref(slot);
    sd_bus_unref(bus);

    return 1;
}
