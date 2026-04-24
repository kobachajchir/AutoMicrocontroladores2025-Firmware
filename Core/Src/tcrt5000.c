#include "tcrt5000.h"

#include <stddef.h>
#include <string.h>

#define TCRT_LINE_SENSOR_COUNT 3U
#define TCRT_OBS_SENSOR_COUNT  5U
#define TCRT_ADC_MAX_VALUE     4095U

static uint16_t TCRT5000_ReadSample(const TCRT5000_Handle_t *h,
                                    const volatile uint16_t *sample,
                                    uint8_t index)
{
    uint16_t value = sample[index];

    if ((h->pull_down_mask & (uint8_t)(1U << index)) != 0U) {
        value = (uint16_t)(TCRT_ADC_MAX_VALUE - value);
    }

    return value;
}

static uint16_t TCRT5000_Median3Filter(TCRT5000_Median3_t *filter, uint16_t sample)
{
    uint16_t a;
    uint16_t b;
    uint16_t c;

    filter->buf[filter->idx] = sample;
    filter->idx = (uint8_t)((filter->idx + 1U) % 3U);

    a = filter->buf[0];
    b = filter->buf[1];
    c = filter->buf[2];

    if (a > b) {
        if (b > c) {
            return b;
        }
        return (a > c) ? c : a;
    }

    if (a > c) {
        return a;
    }

    return (b > c) ? c : b;
}

static uint16_t TCRT5000_EmaFilter(TCRT5000_EMAFast_t *filter, uint16_t sample)
{
    uint32_t acc = ((uint32_t)filter->alpha * sample)
                 + ((uint32_t)(256U - filter->alpha) * filter->prev);

    filter->prev = (uint16_t)(acc >> 8);
    return filter->prev;
}

static uint16_t TCRT5000_MovAvg4Filter(TCRT5000_MovAvg4_t *filter, uint16_t sample)
{
    filter->sum -= filter->buf[filter->idx];
    filter->buf[filter->idx] = sample;
    filter->sum += sample;
    filter->idx = (uint8_t)((filter->idx + 1U) & 0x03U);

    return (uint16_t)(filter->sum >> 2);
}

static void TCRT5000_AdvanceMode(TCRT5000_Handle_t *h)
{
    uint8_t i;

    switch (h->mode) {
    case TCRT_MODE_CALIB_LINE_BLACK:
        h->mode = TCRT_MODE_CALIB_LINE_WHITE;
        for (i = 0U; i < TCRT_LINE_SENSOR_COUNT; i++) {
            h->line_white_sum[i] = 0U;
        }
        h->line_white_count = 0U;
        break;

    case TCRT_MODE_CALIB_LINE_WHITE:
        if ((h->line_black_count != 0U) && (h->line_white_count != 0U)) {
            uint16_t max_threshold = 0U;

            for (i = 0U; i < TCRT_LINE_SENSOR_COUNT; i++) {
                uint32_t avg_black = h->line_black_sum[i] / h->line_black_count;
                uint32_t avg_white = h->line_white_sum[i] / h->line_white_count;
                uint16_t threshold = (uint16_t)((avg_black + avg_white) >> 1);

                if ((i == 0U) || (threshold > max_threshold)) {
                    max_threshold = threshold;
                }
            }
            h->threshold_line = max_threshold;
        }

        h->mode = TCRT_MODE_CALIB_OBSTACLES;
        for (i = 0U; i < TCRT_OBS_SENSOR_COUNT; i++) {
            h->obstacle_free_sum[i] = 0U;
        }
        h->obstacle_count = 0U;
        break;

    case TCRT_MODE_CALIB_OBSTACLES:
        if (h->obstacle_count != 0U) {
            for (i = 0U; i < TCRT_OBS_SENSOR_COUNT; i++) {
                h->threshold_obs[i] =
                    (uint16_t)(h->obstacle_free_sum[i] / h->obstacle_count);
            }
        }

        (void)memset(&h->line_filter, 0, sizeof(h->line_filter));
        (void)memset(&h->lateral_left_filter, 0, sizeof(h->lateral_left_filter));
        (void)memset(&h->lateral_right_filter, 0, sizeof(h->lateral_right_filter));
        (void)memset(&h->obstacle_filters, 0, sizeof(h->obstacle_filters));
        h->lateral_left_filter.alpha = h->lateral_alpha;
        h->lateral_right_filter.alpha = h->lateral_alpha;
        h->ready_sample_count = 0U;
        h->mode = TCRT_MODE_READY;
        break;

    case TCRT_MODE_READY:
    default:
        h->mode = TCRT_MODE_CALIB_LINE_BLACK;
        for (i = 0U; i < TCRT_LINE_SENSOR_COUNT; i++) {
            h->line_black_sum[i] = 0U;
            h->line_white_sum[i] = 0U;
        }
        for (i = 0U; i < TCRT_OBS_SENSOR_COUNT; i++) {
            h->obstacle_free_sum[i] = 0U;
            h->threshold_obs[i] = TCRT5000_DEFAULT_THRESHOLD_OBS;
        }
        h->line_black_count = 0U;
        h->line_white_count = 0U;
        h->obstacle_count = 0U;
        h->ready_sample_count = 0U;
        h->threshold_line = TCRT5000_DEFAULT_THRESHOLD_LINE;
        break;
    }
}

TCRT5000_Status TCRT5000_Init(TCRT5000_Handle_t *h, const TCRT5000_Config_t *config)
{
    uint8_t i;

    if ((h == NULL) || (config == NULL)) {
        return TCRT5000_ERROR;
    }

    (void)memset(h, 0, sizeof(*h));

    h->pull_down_mask = config->pull_down_mask;
    h->lateral_alpha = (config->lateral_alpha == 0U)
        ? TCRT5000_DEFAULT_LATERAL_ALPHA
        : config->lateral_alpha;
    h->auto_mode_advance = config->auto_mode_advance;
    h->light_state = config->light_initial_state;
    h->ready_batch_size = (config->ready_batch_size == 0U)
        ? TCRT5000_DEFAULT_READY_BATCH_SIZE
        : config->ready_batch_size;
    h->threshold_line = (config->default_threshold_line == 0U)
        ? TCRT5000_DEFAULT_THRESHOLD_LINE
        : config->default_threshold_line;
    h->light_write = config->light_write;
    h->light_ctx = config->light_ctx;
    h->mode = TCRT_MODE_IDLE;
    h->flags = h->light_state ? TCRT5000_FLAG_LIGHT : 0U;

    h->lateral_left_filter.alpha = h->lateral_alpha;
    h->lateral_right_filter.alpha = h->lateral_alpha;

    for (i = 0U; i < TCRT_OBS_SENSOR_COUNT; i++) {
        h->threshold_obs[i] = (config->default_threshold_obstacle == 0U)
            ? TCRT5000_DEFAULT_THRESHOLD_OBS
            : config->default_threshold_obstacle;
    }

    if (h->light_write != NULL) {
        h->light_write(h->light_ctx, h->light_state);
    }

    return TCRT5000_OK;
}

TCRT5000_Status TCRT5000_StartCalibration(TCRT5000_Handle_t *h,
                                          uint16_t line_samples,
                                          uint16_t obstacle_samples)
{
    uint8_t i;

    if ((h == NULL) || (line_samples == 0U) || (obstacle_samples == 0U)) {
        return TCRT5000_ERROR;
    }

    for (i = 0U; i < TCRT_LINE_SENSOR_COUNT; i++) {
        h->line_black_sum[i] = 0U;
        h->line_white_sum[i] = 0U;
    }
    for (i = 0U; i < TCRT_OBS_SENSOR_COUNT; i++) {
        h->obstacle_free_sum[i] = 0U;
    }

    (void)memset(&h->line_filter, 0, sizeof(h->line_filter));
    (void)memset(&h->lateral_left_filter, 0, sizeof(h->lateral_left_filter));
    (void)memset(&h->lateral_right_filter, 0, sizeof(h->lateral_right_filter));
    (void)memset(&h->obstacle_filters, 0, sizeof(h->obstacle_filters));
    h->lateral_left_filter.alpha = h->lateral_alpha;
    h->lateral_right_filter.alpha = h->lateral_alpha;

    h->line_black_count = 0U;
    h->line_white_count = 0U;
    h->obstacle_count = 0U;
    h->line_sample_limit = line_samples;
    h->obstacle_sample_limit = obstacle_samples;
    h->ready_sample_count = 0U;
    h->mode_change_pending = false;
    h->mode = TCRT_MODE_CALIB_LINE_BLACK;
    h->flags = h->light_state ? TCRT5000_FLAG_LIGHT : 0U;
    h->threshold_line = TCRT5000_DEFAULT_THRESHOLD_LINE;

    for (i = 0U; i < TCRT_OBS_SENSOR_COUNT; i++) {
        h->threshold_obs[i] = TCRT5000_DEFAULT_THRESHOLD_OBS;
    }

    return TCRT5000_OK;
}

void TCRT5000_RequestModeChange(TCRT5000_Handle_t *h)
{
    if (h != NULL) {
        h->mode_change_pending = true;
    }
}

bool TCRT5000_ProcessSample(TCRT5000_Handle_t *h, const volatile uint16_t *sample)
{
    uint8_t i;

    if ((h == NULL) || (sample == NULL)) {
        return false;
    }

    if (h->mode_change_pending) {
        h->mode_change_pending = false;
        TCRT5000_AdvanceMode(h);
        return false;
    }

    switch (h->mode) {
    case TCRT_MODE_CALIB_LINE_BLACK:
        for (i = 0U; i < TCRT_LINE_SENSOR_COUNT; i++) {
            h->line_black_sum[i] += TCRT5000_ReadSample(h, sample, i);
        }
        h->line_black_count++;
        if (h->auto_mode_advance && (h->line_black_count >= h->line_sample_limit)) {
            TCRT5000_AdvanceMode(h);
        }
        return false;

    case TCRT_MODE_CALIB_LINE_WHITE:
        for (i = 0U; i < TCRT_LINE_SENSOR_COUNT; i++) {
            h->line_white_sum[i] += TCRT5000_ReadSample(h, sample, i);
        }
        h->line_white_count++;
        if (h->auto_mode_advance && (h->line_white_count >= h->line_sample_limit)) {
            TCRT5000_AdvanceMode(h);
        }
        return false;

    case TCRT_MODE_CALIB_OBSTACLES:
        for (i = 0U; i < TCRT_OBS_SENSOR_COUNT; i++) {
            h->obstacle_free_sum[i] += TCRT5000_ReadSample(
                h,
                sample,
                (uint8_t)(i + TCRT_LINE_SENSOR_COUNT)
            );
        }
        h->obstacle_count++;
        if (h->auto_mode_advance && (h->obstacle_count >= h->obstacle_sample_limit)) {
            TCRT5000_AdvanceMode(h);
        }
        return false;

    case TCRT_MODE_READY:
    {
        uint16_t line_value = TCRT5000_Median3Filter(
            &h->line_filter,
            TCRT5000_ReadSample(h, sample, 0U)
        );
        uint16_t left_value = TCRT5000_EmaFilter(
            &h->lateral_left_filter,
            TCRT5000_ReadSample(h, sample, 1U)
        );
        uint16_t right_value = TCRT5000_EmaFilter(
            &h->lateral_right_filter,
            TCRT5000_ReadSample(h, sample, 2U)
        );
        uint8_t flags = h->light_state ? TCRT5000_FLAG_LIGHT : 0U;

        if (line_value >= h->threshold_line) {
            if (right_value < h->threshold_line) {
                flags |= TCRT5000_FLAG_TOO_LEFT;
            }
            if (left_value < h->threshold_line) {
                flags |= TCRT5000_FLAG_TOO_RIGHT;
            }
        }

        for (i = 0U; i < TCRT_OBS_SENSOR_COUNT; i++) {
            uint16_t obstacle_value = TCRT5000_MovAvg4Filter(
                &h->obstacle_filters[i],
                TCRT5000_ReadSample(h, sample, (uint8_t)(i + TCRT_LINE_SENSOR_COUNT))
            );

            if (obstacle_value < h->threshold_obs[i]) {
                flags |= (uint8_t)(1U << (i + 2U));
            }
        }

        h->flags = flags;
        h->ready_sample_count++;
        if (h->ready_sample_count >= h->ready_batch_size) {
            h->ready_sample_count = 0U;
            return true;
        }
        return false;
    }

    case TCRT_MODE_IDLE:
    default:
        return false;
    }
}
