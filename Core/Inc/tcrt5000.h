#ifndef TCRT5000_H_
#define TCRT5000_H_

#include <stdbool.h>
#include <stdint.h>

#define TCRT_MODE_IDLE              0U
#define TCRT_MODE_CALIB_LINE_BLACK  1U
#define TCRT_MODE_CALIB_LINE_WHITE  2U
#define TCRT_MODE_CALIB_OBSTACLES   3U
#define TCRT_MODE_READY             4U

#define TCRT5000_NUM_SENSORS              8U
#define TCRT5000_DEFAULT_READY_BATCH_SIZE 4000U
#define TCRT5000_DEFAULT_LATERAL_ALPHA    200U
#define TCRT5000_DEFAULT_THRESHOLD_LINE   2000U
#define TCRT5000_DEFAULT_THRESHOLD_OBS    2000U

#define TCRT5000_FLAG_TOO_LEFT        (1U << 0)
#define TCRT5000_FLAG_TOO_RIGHT       (1U << 1)
#define TCRT5000_FLAG_OBS_DIAG_LEFT   (1U << 2)
#define TCRT5000_FLAG_OBS_FRONT_LEFT  (1U << 3)
#define TCRT5000_FLAG_OBS_CENTER      (1U << 4)
#define TCRT5000_FLAG_OBS_FRONT_RIGHT (1U << 5)
#define TCRT5000_FLAG_OBS_DIAG_RIGHT  (1U << 6)
#define TCRT5000_FLAG_LIGHT           (1U << 7)

typedef enum {
    TCRT5000_OK = 0U,
    TCRT5000_ERROR = 1U
} TCRT5000_Status;

typedef void (*TCRT_LightWriteFn)(void *ctx, bool state);

typedef struct {
    uint8_t pull_down_mask;
    uint8_t lateral_alpha;
    bool auto_mode_advance;
    bool light_initial_state;
    uint16_t ready_batch_size;
    uint16_t default_threshold_line;
    uint16_t default_threshold_obstacle;
    TCRT_LightWriteFn light_write;
    void *light_ctx;
} TCRT5000_Config_t;

typedef struct {
    uint16_t buf[3];
    uint8_t idx;
} TCRT5000_Median3_t;

typedef struct {
    uint16_t prev;
    uint8_t alpha;
} TCRT5000_EMAFast_t;

typedef struct {
    uint16_t buf[4];
    uint8_t idx;
    uint32_t sum;
} TCRT5000_MovAvg4_t;

typedef struct {
    uint32_t line_black_sum[3];
    uint32_t line_white_sum[3];
    uint32_t obstacle_free_sum[5];
    TCRT5000_Median3_t line_filter;
    TCRT5000_EMAFast_t lateral_left_filter;
    TCRT5000_EMAFast_t lateral_right_filter;
    TCRT5000_MovAvg4_t obstacle_filters[5];
    TCRT_LightWriteFn light_write;
    void *light_ctx;
    uint16_t line_black_count;
    uint16_t line_white_count;
    uint16_t obstacle_count;
    uint16_t line_sample_limit;
    uint16_t obstacle_sample_limit;
    uint16_t ready_sample_count;
    uint16_t ready_batch_size;
    uint16_t threshold_line;
    uint16_t threshold_obs[5];
    uint8_t pull_down_mask;
    uint8_t lateral_alpha;
    uint8_t mode;
    uint8_t flags;
    bool auto_mode_advance;
    bool mode_change_pending;
    bool light_state;
} TCRT5000_Handle_t;

typedef TCRT5000_Handle_t TCRTHandlerTask;

TCRT5000_Status TCRT5000_Init(TCRT5000_Handle_t *h, const TCRT5000_Config_t *config);
TCRT5000_Status TCRT5000_StartCalibration(TCRT5000_Handle_t *h,
                                          uint16_t line_samples,
                                          uint16_t obstacle_samples);
void TCRT5000_RequestModeChange(TCRT5000_Handle_t *h);
bool TCRT5000_ProcessSample(TCRT5000_Handle_t *h, const volatile uint16_t *sample);

#endif /* TCRT5000_H_ */
