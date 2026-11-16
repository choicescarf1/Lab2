#include <stdio.h>
#include <string.h>
#include <systemd/sd-bus.h>

// 서버와 동일한 D-Bus 정보 사용
#define SERVICE "com.example.Lab2.Notify"
#define OBJECT_PATH "/com/example/Lab2/Notify"
#define INTERFACE "com.example.Lab2.Notify"

int main(int argc, char *argv[]) {

    // 1. 명령행 인자 확인
    if (argc != 2) {

        fprintf(
            stderr,
            "사용법: %s <알림 메시지>\n",
            argv[0]
        );

        return 1;
    }

    sd_bus *bus = NULL;
    sd_bus_message *reply = NULL;

    sd_bus_error error = SD_BUS_ERROR_NULL;

    // 2. D-Bus 세션 버스에 연결
    int r = sd_bus_default_user(&bus);

    if (r < 0) {

        fprintf(
            stderr,
            "세션 버스 연결 실패: %s\n",
            strerror(-r)
        );

        return 1;
    }

    // 3. 서버의 Notify 메서드 호출
    r = sd_bus_call_method(
        bus,
        SERVICE,
        OBJECT_PATH,
        INTERFACE,
        "Notify",
        &error,
        &reply,
        "s",
        argv[1]
    );

    if (r < 0) {

        fprintf(
            stderr,
            "알림 전송 실패: %s\n",
            error.message
                ? error.message
                : strerror(-r)
        );

        goto fail;
    }

    // 4. 서버 응답 읽기
    const char *response = NULL;

    r = sd_bus_message_read(
        reply,
        "s",
        &response
    );

    if (r < 0) {

        fprintf(
            stderr,
            "서버 응답 읽기 실패: %s\n",
            strerror(-r)
        );

        goto fail;
    }

    // 5. 응답 출력
    printf("[서버 응답] %s\n", response);

    // 6. 자원 정리
    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);

    return 0;

fail:
    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);

    return 1;
}
