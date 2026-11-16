#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <time.h>

// 타자 연습에 사용할 문장
static const char *sentences[] = {
    "Linux is an open source operating system.",
    "A process can communicate using a pipe.",
    "Rust provides memory safety with ownership."
};

// 경과 시간 계산 함수
static double elapsed_seconds(
    const struct timespec *begin,
    const struct timespec *end
) {
    return (double)(end->tv_sec - begin->tv_sec)
        + (double)(end->tv_nsec - begin->tv_nsec) / 1e9;
}

// 잘못 입력한 문자 개수 계산 함수
static size_t count_typing_errors(
    const char *expected,
    const char *typed
) {
    size_t expected_len = strlen(expected);
    size_t typed_len = strlen(typed);

    size_t count =
        expected_len > typed_len
        ? expected_len
        : typed_len;

    size_t errors = 0;

    for (size_t i = 0; i < count; ++i) {

        if (i >= expected_len ||
            i >= typed_len ||
            expected[i] != typed[i]) {

            ++errors;
        }
    }

    return errors;
}

// 메인 함수
int main(void) {

    // 연습 문장 개수 계산
    const size_t n =
        sizeof(sentences) / sizeof(sentences[0]);

    char answer[1024];

    size_t total_typed = 0;
    size_t total_errors = 0;
    size_t compared_chars = 0;

    double total_seconds = 0;

    printf(
        "타자 연습 (%zu문장)\n",
        n
    );

    printf(
        "문장을 똑같이 입력한 다음 Enter를 누르세요.\n\n"
    );

    // 문장별 타자 연습
    for (size_t i = 0; i < n; ++i) {

        printf(
            "문장 %zu/%zu: %s\n입력 > ",
            i + 1,
            n,
            sentences[i]
        );

        fflush(stdout);

        struct timespec before, after;

        // 입력 시작 시간 측정
        if (clock_gettime(
                CLOCK_MONOTONIC,
                &before
            ) < 0) {

            perror("clock_gettime");
            return 1;
        }

        // 사용자 입력
        if (!fgets(answer, sizeof(answer), stdin)) {

            fprintf(
                stderr,
                "입력이 종료되었습니다.\n"
            );

            return 1;
        }

        // 입력 종료 시간 측정
        if (clock_gettime(
                CLOCK_MONOTONIC,
                &after
            ) < 0) {

            perror("clock_gettime");
            return 1;
        }

        // 줄바꿈 문자 제거
        size_t typed_len = strcspn(answer, "\n");

        if (answer[typed_len] == '\n') {
            answer[typed_len] = '\0';
        }

        else if (!feof(stdin)) {

            int c;

            while ((c = getchar()) != '\n' &&
                   c != EOF) {}

            fprintf(
                stderr,
                "입력 문장이 너무 깁니다(최대 1023바이트).\n"
            );

            return 1;
        }

        // 오타 계산
        size_t expected_len = strlen(sentences[i]);

        size_t errors = count_typing_errors(
            sentences[i],
            answer
        );

        // 경과 시간 계산
        double seconds = elapsed_seconds(
            &before,
            &after
        );

        if (seconds < 0.01)
            seconds = 0.01;

        // 전체 결과 누적
        total_seconds += seconds;
        total_typed += typed_len;
        total_errors += errors;

        compared_chars +=
            expected_len > typed_len
            ? expected_len
            : typed_len;

        // 문장별 결과 출력
        printf(
            "오타: %zu회 / 소요 시간: %.2f초 / 분당 타수: %.1f\n\n",
            errors,
            seconds,
            typed_len * 60.0 / seconds
        );
    }

    // 평균 분당 타수 계산
    double cpm =
        total_typed * 60.0 / total_seconds;

    // 평균 분당 단어 수 계산
    double wpm = cpm / 5.0;

    // 정확도 계산
    double accuracy = compared_chars
        ? 100.0 * (
            1.0 - (double)total_errors / compared_chars
        )
        : 100.0;

    // 최종 결과 출력
    printf("[최종 결과]\n");

    printf(
        "잘못 타이핑한 문자: %zu회\n",
        total_errors
    );

    printf(
        "총 소요 시간: %.2f초\n",
        total_seconds
    );

    printf(
        "평균 분당 타수: %.1f CPM\n",
        cpm
    );

    printf(
        "평균 분당 단어 수: %.1f WPM\n",
        wpm
    );

    printf(
        "정확도: %.1f%%\n",
        accuracy
    );

    return 0;
}
